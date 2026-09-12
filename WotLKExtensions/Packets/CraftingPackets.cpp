#include "CraftingPackets.h"
#include "Packet.h"

#include <ClientData/GameObject.h>
#include <ClientData/ObjectManager.h>
#include <ClientDetours.h>
#include <CustomLua.h>
#include <CustomPacket.h>
#include <Item.h>
#include <Lua/XMLExtensions.h>

#include <Windows.h>

namespace
{
	constexpr uint32_t kObjectFieldGuid = 0;
	constexpr uint32_t kObjectFieldEntry = 3;
	constexpr uint32_t kItemFieldStackCount = 14;
	constexpr uint32_t kContainerFieldNumSlots = 64;
	constexpr uint32_t kItemLockFlagOffset = 0x394;
	constexpr uint32_t kPlayerBackpackOffset = 0x18F0;
	constexpr int kBackpackFirstIndex = 23;
	constexpr int kBackpackLastIndex = 38;
	constexpr int kBankFirstIndex = 39;
	constexpr int kBankLastIndex = 66;
	constexpr int kKeyringFirstIndex = 86;
	constexpr int kKeyringLastIndex = 117;
	constexpr int kMaxEquippedBags = 4;
	constexpr int kMaxContainers = 10;
	constexpr uint32_t kBagVTableSlot = 10;
	constexpr uint32_t kRangeCheckIntervalMs = 500;
	constexpr uint32_t kPendingTimeoutMs = 15000;
	constexpr uint8_t kScrapItemSkipped = 3;
	constexpr float kRangeTolerance = 1.5f;
	constexpr uint8_t kOpenFlagCanCraft = 0x01;
	constexpr uint8_t kResultTimedOut = 255;

	uint64_t MakeRecipeKey(uint32_t pattern, uint32_t material)
	{
		return (static_cast<uint64_t>(pattern) << 32) | material;
	}

	uint64_t ItemGuid(void* item)
	{
		return static_cast<CGObject_C*>(item)->GetValue<uint64_t>(kObjectFieldGuid);
	}

	uint32_t ItemEntry(void* item)
	{
		return static_cast<CGObject_C*>(item)->GetValue<uint32_t>(kObjectFieldEntry);
	}

	uint32_t ItemStackCount(void* item)
	{
		return static_cast<CGObject_C*>(item)->GetValue<uint32_t>(kItemFieldStackCount);
	}

	void* ContainerFromObject(CGObject_C* container)
	{
		typedef void*(__fastcall * GetBagFn)(void* self, void* edx);
		void** vtable = *reinterpret_cast<void***>(container);
		return reinterpret_cast<GetBagFn>(vtable[kBagVTableSlot])(container, nullptr);
	}

	int OptionalSlotArg(lua_State* L, int index, int maxSlots)
	{
		if (FrameScript::GetTop(L) < index || !FrameScript::IsNumber(L, index))
			return -1;

		int slot = static_cast<int>(FrameScript::GetNumber(L, index)) - 1;
		if (slot < 0 || slot >= maxSlots)
			return -1;

		return slot;
	}
}

CraftingPackets& CraftingPackets::Instance()
{
	static CraftingPackets instance;
	return instance;
}

bool CraftingPackets::IsItemLocked(void* item)
{
	return (*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(item) + kItemLockFlagOffset) & 1) != 0;
}

void CraftingPackets::LockItem(uint64_t guid)
{
	if (guid)
		CGGameUI::LockItem(guid);
}

void CraftingPackets::UnlockItem(uint64_t guid)
{
	if (guid)
		CGGameUI::UnlockItem(guid);
}

void CraftingPackets::SignalError(ErrorCode code)
{
	FrameXMLExtensions::SignalEvent("HOT_CRAFTING_ERROR", "%d", static_cast<int>(code));
}

void CraftingPackets::SignalSlots(Mode mode)
{
	FrameXMLExtensions::SignalEvent("HOT_CRAFTING_SLOTS_UPDATE", "%d", static_cast<int>(mode));
}

void* CraftingPackets::ResolveContainerItem(int luaBag, int luaSlot)
{
	if (luaSlot < 1)
		return nullptr;

	int index = luaSlot - 1;
	void* container = nullptr;

	if (luaBag <= 0)
	{
		CGObject_C* player = ClientData::ObjectManager::GetActivePlayerObject();
		if (!player)
			return nullptr;

		container = reinterpret_cast<uint8_t*>(player) + kPlayerBackpackOffset;
		switch (luaBag)
		{
			case 0:
				index += kBackpackFirstIndex;
				if (index > kBackpackLastIndex)
					return nullptr;
				break;
			case -1:
				index += kBankFirstIndex;
				if (index > kBankLastIndex)
					return nullptr;
				break;
			case -2:
				index += kKeyringFirstIndex;
				if (index > kKeyringLastIndex)
					return nullptr;
				break;
			default:
				return nullptr;
		}
	}
	else
	{
		if (luaBag > kMaxContainers)
			return nullptr;

		uint64_t bagGuid = CGContainerInfo::GetContainer(luaBag - 1);
		if (!bagGuid)
			return nullptr;

		CGObject_C* bagObject = ClientData::ObjectManager::ObjectPtr(bagGuid, TYPEMASK_CONTAINER);
		if (!bagObject)
			return nullptr;

		if (index >= static_cast<int>(bagObject->GetValue<uint32_t>(kContainerFieldNumSlots)))
			return nullptr;

		container = ContainerFromObject(bagObject);
		if (!container)
			return nullptr;
	}

	return CGBag_C::GetItemPointer(container, index);
}

bool CraftingPackets::FindItemBagSlot(uint64_t guid, int& outBag, int& outSlot)
{
	CGObject_C* player = ClientData::ObjectManager::GetActivePlayerObject();
	if (!player)
		return false;

	void* backpack = reinterpret_cast<uint8_t*>(player) + kPlayerBackpackOffset;
	for (int index = kBackpackFirstIndex; index <= kBackpackLastIndex; ++index)
	{
		void* item = CGBag_C::GetItemPointer(backpack, index);
		if (item && ItemGuid(item) == guid)
		{
			outBag = 0;
			outSlot = index - kBackpackFirstIndex + 1;
			return true;
		}
	}

	for (int bag = 0; bag < kMaxEquippedBags; ++bag)
	{
		uint64_t bagGuid = CGContainerInfo::GetContainer(bag);
		if (!bagGuid)
			continue;

		CGObject_C* bagObject = ClientData::ObjectManager::ObjectPtr(bagGuid, TYPEMASK_CONTAINER);
		if (!bagObject)
			continue;

		void* container = ContainerFromObject(bagObject);
		if (!container)
			continue;

		uint32_t numSlots = bagObject->GetValue<uint32_t>(kContainerFieldNumSlots);
		for (uint32_t index = 0; index < numSlots; ++index)
		{
			void* item = CGBag_C::GetItemPointer(container, static_cast<int>(index));
			if (item && ItemGuid(item) == guid)
			{
				outBag = bag + 1;
				outSlot = static_cast<int>(index) + 1;
				return true;
			}
		}
	}

	return false;
}

bool CraftingPackets::ResolveItem(uint64_t guid, ItemRef& out)
{
	out = ItemRef();
	if (!guid)
		return false;

	CGObject_C* item = ClientData::ObjectManager::ObjectPtr(guid, TYPEMASK_ITEM);
	if (!item)
		return false;

	out.guid = guid;
	out.entry = ItemEntry(item);
	out.count = ItemStackCount(item);
	FindItemBagSlot(guid, out.bag, out.slot);
	return true;
}

bool CraftingPackets::IsScrapCandidate(uint32_t entry)
{
	ItemRecord* record = Item::GetItemRecord(static_cast<int>(entry));
	if (!record)
		return true;

	switch (record->itemClass)
	{
		case 0:
		case 1:
		case 6:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 16:
			return false;
		default:
			return true;
	}
}

bool CraftingPackets::IsCatalystEntry(uint32_t entry) const
{
	return m_catalystLookup.find(entry) != m_catalystLookup.end();
}

bool CraftingPackets::HasCatalyst(uint64_t guid) const
{
	for (uint64_t slotGuid : m_catalysts)
		if (slotGuid && slotGuid == guid)
			return true;
	return false;
}

bool CraftingPackets::HasScrapItem(uint64_t guid) const
{
	for (uint64_t slotGuid : m_scrapItems)
		if (slotGuid && slotGuid == guid)
			return true;
	return false;
}

bool CraftingPackets::PlaceCatalyst(uint64_t guid, uint32_t entry, int preferredSlot)
{
	if (m_craftPending || m_scrapPending)
	{
		SignalError(CRAFT_ERR_BUSY);
		return false;
	}

	if (!IsCatalystEntry(entry))
	{
		SignalError(CRAFT_ERR_NOT_A_CATALYST);
		return false;
	}

	if (HasCatalyst(guid))
	{
		SignalError(CRAFT_ERR_ALREADY_ADDED);
		return false;
	}

	int slot = -1;
	if (preferredSlot >= 0 && preferredSlot < static_cast<int>(MaxCatalysts))
		slot = preferredSlot;
	else
	{
		for (uint32_t i = 0; i < MaxCatalysts; ++i)
		{
			if (!m_catalysts[i])
			{
				slot = static_cast<int>(i);
				break;
			}
		}
	}

	if (slot < 0)
	{
		SignalError(CRAFT_ERR_SLOTS_FULL);
		return false;
	}

	if (m_catalysts[slot])
		UnlockItem(m_catalysts[slot]);

	m_catalysts[slot] = guid;
	SignalSlots(MODE_CRAFTING);
	return true;
}

bool CraftingPackets::PlaceScrapItem(uint64_t guid, uint32_t entry, int preferredSlot)
{
	if (m_craftPending || m_scrapPending)
	{
		SignalError(CRAFT_ERR_BUSY);
		return false;
	}

	if (!IsScrapCandidate(entry))
	{
		SignalError(CRAFT_ERR_CANNOT_SCRAP);
		return false;
	}

	if (HasScrapItem(guid))
	{
		SignalError(CRAFT_ERR_ALREADY_ADDED);
		return false;
	}

	int slot = -1;
	if (preferredSlot >= 0 && preferredSlot < static_cast<int>(MaxScrapItems))
		slot = preferredSlot;
	else
	{
		for (uint32_t i = 0; i < MaxScrapItems; ++i)
		{
			if (!m_scrapItems[i])
			{
				slot = static_cast<int>(i);
				break;
			}
		}
	}

	if (slot < 0)
	{
		SignalError(CRAFT_ERR_SLOTS_FULL);
		return false;
	}

	if (m_scrapItems[slot])
		UnlockItem(m_scrapItems[slot]);

	m_scrapItems[slot] = guid;
	SignalSlots(MODE_SCRAPPING);
	return true;
}

bool CraftingPackets::HandleUseContainerItem(lua_State* L)
{
	if (!m_open)
		return false;

	if (!FrameScript::IsNumber(L, 1) || !FrameScript::IsNumber(L, 2))
		return false;

	int bag = static_cast<int>(FrameScript::GetNumber(L, 1));
	int slot = static_cast<int>(FrameScript::GetNumber(L, 2));
	void* item = ResolveContainerItem(bag, slot);
	if (!item)
		return false;

	if (IsItemLocked(item))
	{
		SignalError(CRAFT_ERR_ITEM_LOCKED);
		return true;
	}

	uint64_t guid = ItemGuid(item);
	uint32_t entry = ItemEntry(item);
	bool placed = m_mode == MODE_CRAFTING ? PlaceCatalyst(guid, entry, -1) : PlaceScrapItem(guid, entry, -1);
	if (placed)
		LockItem(guid);

	return true;
}

bool CraftingPackets::AddCursorItem(Mode mode, int preferredSlot)
{
	if (!m_open)
		return false;

	uint64_t guid = CGGameUI::GetCursorItem();
	if (!guid)
		return false;

	CGObject_C* item = ClientData::ObjectManager::ObjectPtr(guid, TYPEMASK_ITEM);
	if (!item)
		return false;

	uint32_t entry = ItemEntry(item);
	bool placed = mode == MODE_CRAFTING ? PlaceCatalyst(guid, entry, preferredSlot) : PlaceScrapItem(guid, entry, preferredSlot);
	if (!placed)
		return false;

	CGGameUI::ClearCursor(1, 1);
	LockItem(guid);
	return true;
}

void CraftingPackets::UnlockAllSlots()
{
	for (uint64_t& guid : m_catalysts)
		UnlockItem(guid);
	for (uint64_t& guid : m_scrapItems)
		UnlockItem(guid);
}

void CraftingPackets::ClearSlots()
{
	for (uint64_t& guid : m_catalysts)
		guid = 0;
	for (uint64_t& guid : m_scrapItems)
		guid = 0;
}

void CraftingPackets::CloseInternal(bool notifyServer, bool signal)
{
	if (!m_open)
		return;

	UnlockAllSlots();
	ClearSlots();
	m_open = false;
	m_station = 0;
	m_flags = 0;
	m_craftPending = false;
	m_scrapPending = false;

	if (notifyServer)
		Packet(CMSG_CRAFTING_CLOSE).Send();

	if (signal)
		FrameXMLExtensions::SignalEvent("HOT_CRAFTING_CLOSE", "");
}

void CraftingPackets::Reset()
{
	m_open = false;
	m_station = 0;
	m_flags = 0;
	m_mode = MODE_CRAFTING;
	m_pattern = 0;
	m_material = 0;
	m_craftPending = false;
	m_scrapPending = false;
	ClearSlots();
}

bool CraftingPackets::SendCraft()
{
	if (!m_open || m_craftPending || m_scrapPending)
		return false;

	if (!m_pattern || !m_material || !(m_flags & kOpenFlagCanCraft))
		return false;

	if (m_recipeLookup.find(MakeRecipeKey(m_pattern, m_material)) == m_recipeLookup.end())
		return false;

	Packet packet(CMSG_CRAFTING_CRAFT);
	packet.PutUInt32(m_pattern);
	packet.PutUInt32(m_material);
	packet.PutUInt64(m_catalysts[0]);
	packet.PutUInt64(m_catalysts[1]);
	packet.Send();

	m_craftPending = true;
	m_craftPendingUntil = GetTickCount() + kPendingTimeoutMs;
	return true;
}

bool CraftingPackets::SendScrap()
{
	if (!m_open || m_craftPending || m_scrapPending)
		return false;

	uint8_t count = 0;
	for (uint64_t guid : m_scrapItems)
		if (guid)
			++count;

	if (!count)
		return false;

	Packet packet(CMSG_CRAFTING_SCRAP);
	packet.PutUInt8(count);
	for (uint64_t guid : m_scrapItems)
		if (guid)
			packet.PutUInt64(guid);
	packet.Send();

	m_scrapPending = true;
	m_scrapPendingUntil = GetTickCount() + kPendingTimeoutMs;
	return true;
}

void CraftingPackets::CheckStationRange()
{
	CGObject_C* player = ClientData::ObjectManager::GetActivePlayerObject();
	CGObject_C* station = m_station ? ClientData::ObjectManager::ObjectPtr(m_station, TYPEMASK_GAMEOBJECT) : nullptr;
	if (!player || !station || player->distance(station) > m_interactRange + kRangeTolerance)
	{
		CloseInternal(true, true);
		return;
	}

	bool catalystsChanged = false;
	for (uint64_t& guid : m_catalysts)
	{
		if (guid && !ClientData::ObjectManager::ObjectPtr(guid, TYPEMASK_ITEM))
		{
			guid = 0;
			catalystsChanged = true;
		}
	}

	bool scrapChanged = false;
	for (uint64_t& guid : m_scrapItems)
	{
		if (guid && !ClientData::ObjectManager::ObjectPtr(guid, TYPEMASK_ITEM))
		{
			guid = 0;
			scrapChanged = true;
		}
	}

	if (catalystsChanged)
		SignalSlots(MODE_CRAFTING);
	if (scrapChanged)
		SignalSlots(MODE_SCRAPPING);
}

void CraftingPackets::CheckPendingTimeouts()
{
	uint32_t now = GetTickCount();
	if (m_craftPending && static_cast<int32_t>(now - m_craftPendingUntil) >= 0)
	{
		m_craftPending = false;
		FrameXMLExtensions::SignalEvent("HOT_CRAFTING_RESULT", "%d%d", static_cast<int>(kResultTimedOut), 0);
	}

	if (m_scrapPending && static_cast<int32_t>(now - m_scrapPendingUntil) >= 0)
	{
		m_scrapPending = false;
		FrameXMLExtensions::SignalEvent("HOT_CRAFTING_SCRAP_RESULT", "%d%d%d", static_cast<int>(kResultTimedOut), 0, 0);
	}
}

void CraftingPackets::Tick()
{
	if (!m_open)
		return;

	uint32_t now = GetTickCount();
	if (static_cast<int32_t>(now - m_nextRangeCheck) >= 0)
	{
		m_nextRangeCheck = now + kRangeCheckIntervalMs;
		CheckStationRange();
		if (!m_open)
			return;
	}

	CheckPendingTimeouts();
}

void CraftingPackets::Handler_SMSG_CRAFTING_DATA(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	CraftingPackets& self = Instance();

	self.m_dataVersion = r.GetUInt32();
	self.m_scrapCurrency = r.GetUInt32();
	self.m_interactRange = r.GetFloat();

	self.m_patterns.clear();
	self.m_materials.clear();
	self.m_recipes.clear();
	self.m_catalystList.clear();
	self.m_recipeLookup.clear();
	self.m_catalystLookup.clear();

	uint8_t patternCount = r.GetUInt8();
	self.m_patterns.reserve(patternCount);
	for (uint8_t i = 0; i < patternCount; ++i)
	{
		PatternInfo info;
		info.entry = r.GetUInt32();
		info.category = r.GetUInt8();
		self.m_patterns.push_back(info);
	}

	uint8_t materialCount = r.GetUInt8();
	self.m_materials.reserve(materialCount);
	for (uint8_t i = 0; i < materialCount; ++i)
	{
		MaterialInfo info;
		info.entry = r.GetUInt32();
		info.category = r.GetUInt8();
		self.m_materials.push_back(info);
	}

	uint16_t recipeCount = r.GetUInt16();
	self.m_recipes.reserve(recipeCount);
	for (uint16_t i = 0; i < recipeCount; ++i)
	{
		RecipeInfo info;
		info.pattern = r.GetUInt32();
		info.material = r.GetUInt32();
		info.result = r.GetUInt32();
		info.scrapCost = r.GetUInt32();
		self.m_recipes.push_back(info);
		self.m_recipeLookup[MakeRecipeKey(info.pattern, info.material)] = info;
	}

	uint8_t catalystCount = r.GetUInt8();
	self.m_catalystList.reserve(catalystCount);
	for (uint8_t i = 0; i < catalystCount; ++i)
	{
		CatalystInfo info;
		info.entry = r.GetUInt32();
		info.type = r.GetUInt8();
		info.value1 = r.GetInt32();
		info.value2 = r.GetInt32();
		self.m_catalystList.push_back(info);
		self.m_catalystLookup[info.entry] = info;
	}

	FrameXMLExtensions::SignalEvent("HOT_CRAFTING_DATA_UPDATE", "%d", static_cast<int>(self.m_dataVersion));
}

void CraftingPackets::Handler_SMSG_CRAFTING_OPEN(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint64_t station = r.GetUInt64();
	r.GetUInt32();
	uint8_t flags = r.GetUInt8();

	CraftingPackets& self = Instance();
	if (self.m_open)
		self.UnlockAllSlots();

	self.ClearSlots();
	self.m_open = true;
	self.m_station = station;
	self.m_flags = flags;
	self.m_mode = MODE_CRAFTING;
	self.m_pattern = 0;
	self.m_material = 0;
	self.m_craftPending = false;
	self.m_scrapPending = false;
	self.m_nextRangeCheck = GetTickCount() + kRangeCheckIntervalMs;

	FrameXMLExtensions::SignalEvent("HOT_CRAFTING_OPEN", "%d", static_cast<int>(flags));
}

void CraftingPackets::Handler_SMSG_CRAFTING_CLOSE(void*, uint32_t, uint32_t, CDataStore*)
{
	Instance().CloseInternal(false, true);
}

void CraftingPackets::Handler_SMSG_CRAFTING_RESULT(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint8_t code = r.GetUInt8();
	uint32_t resultEntry = r.GetUInt32();

	CraftingPackets& self = Instance();
	self.m_craftPending = false;

	if (code == 0)
	{
		for (uint64_t& guid : self.m_catalysts)
		{
			UnlockItem(guid);
			guid = 0;
		}
		SignalSlots(MODE_CRAFTING);
	}

	FrameXMLExtensions::SignalEvent("HOT_CRAFTING_RESULT", "%d%d", static_cast<int>(code), static_cast<int>(resultEntry));
}

void CraftingPackets::Handler_SMSG_CRAFTING_SCRAP_RESULT(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint8_t code = r.GetUInt8();
	uint8_t count = r.GetUInt8();

	CraftingPackets& self = Instance();
	self.m_scrapPending = false;

	int scrapped = 0;
	int rejected = 0;
	bool changed = false;
	for (uint8_t i = 0; i < count; ++i)
	{
		uint64_t guid = r.GetUInt64();
		uint8_t result = r.GetUInt8();
		if (result == kScrapItemSkipped)
			continue;

		if (result == 0)
			++scrapped;
		else
			++rejected;

		for (uint64_t& slotGuid : self.m_scrapItems)
		{
			if (slotGuid == guid)
			{
				UnlockItem(slotGuid);
				slotGuid = 0;
				changed = true;
			}
		}
	}

	if (changed)
		SignalSlots(MODE_SCRAPPING);

	FrameXMLExtensions::SignalEvent("HOT_CRAFTING_SCRAP_RESULT", "%d%d%d", static_cast<int>(code), scrapped, rejected);
}

int CraftingPackets::Script_CraftingIsOpen(lua_State* L)
{
	FrameScript::PushBoolean(L, Instance().m_open ? 1 : 0);
	return 1;
}

int CraftingPackets::Script_CraftingClose(lua_State*)
{
	Instance().CloseInternal(true, true);
	return 0;
}

int CraftingPackets::Script_CraftingCanCraft(lua_State* L)
{
	CraftingPackets& self = Instance();
	FrameScript::PushBoolean(L, (self.m_open && (self.m_flags & kOpenFlagCanCraft)) ? 1 : 0);
	return 1;
}

int CraftingPackets::Script_CraftingGetMode(lua_State* L)
{
	FrameScript::PushNumber(L, Instance().m_mode);
	return 1;
}

int CraftingPackets::Script_CraftingSetMode(lua_State* L)
{
	if (FrameScript::GetTop(L) >= 1 && FrameScript::IsNumber(L, 1))
	{
		int mode = static_cast<int>(FrameScript::GetNumber(L, 1));
		if (mode == MODE_CRAFTING || mode == MODE_SCRAPPING)
			Instance().m_mode = static_cast<Mode>(mode);
	}
	return 0;
}

int CraftingPackets::Script_CraftingGetDataVersion(lua_State* L)
{
	FrameScript::PushNumber(L, Instance().m_dataVersion);
	return 1;
}

int CraftingPackets::Script_CraftingGetScrapCurrency(lua_State* L)
{
	FrameScript::PushNumber(L, Instance().m_scrapCurrency);
	return 1;
}

int CraftingPackets::Script_CraftingGetNumPatterns(lua_State* L)
{
	FrameScript::PushNumber(L, static_cast<double>(Instance().m_patterns.size()));
	return 1;
}

int CraftingPackets::Script_CraftingGetPatternInfo(lua_State* L)
{
	CraftingPackets& self = Instance();
	int index = OptionalSlotArg(L, 1, static_cast<int>(self.m_patterns.size()));
	if (index < 0)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	const PatternInfo& info = self.m_patterns[index];
	FrameScript::PushNumber(L, info.entry);
	FrameScript::PushNumber(L, info.category);
	return 2;
}

int CraftingPackets::Script_CraftingGetNumMaterials(lua_State* L)
{
	FrameScript::PushNumber(L, static_cast<double>(Instance().m_materials.size()));
	return 1;
}

int CraftingPackets::Script_CraftingGetMaterialInfo(lua_State* L)
{
	CraftingPackets& self = Instance();
	int index = OptionalSlotArg(L, 1, static_cast<int>(self.m_materials.size()));
	if (index < 0)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	const MaterialInfo& info = self.m_materials[index];
	FrameScript::PushNumber(L, info.entry);
	FrameScript::PushNumber(L, info.category);
	return 2;
}

int CraftingPackets::Script_CraftingGetNumRecipes(lua_State* L)
{
	FrameScript::PushNumber(L, static_cast<double>(Instance().m_recipes.size()));
	return 1;
}

int CraftingPackets::Script_CraftingGetRecipeInfo(lua_State* L)
{
	CraftingPackets& self = Instance();
	int index = OptionalSlotArg(L, 1, static_cast<int>(self.m_recipes.size()));
	if (index < 0)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	const RecipeInfo& info = self.m_recipes[index];
	FrameScript::PushNumber(L, info.pattern);
	FrameScript::PushNumber(L, info.material);
	FrameScript::PushNumber(L, info.result);
	FrameScript::PushNumber(L, info.scrapCost);
	return 4;
}

int CraftingPackets::Script_CraftingGetRecipe(lua_State* L)
{
	if (FrameScript::GetTop(L) < 2 || !FrameScript::IsNumber(L, 1) || !FrameScript::IsNumber(L, 2))
	{
		FrameScript::PushNil(L);
		return 1;
	}

	uint32_t pattern = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));
	uint32_t material = static_cast<uint32_t>(FrameScript::GetNumber(L, 2));
	CraftingPackets& self = Instance();
	auto it = self.m_recipeLookup.find(MakeRecipeKey(pattern, material));
	if (it == self.m_recipeLookup.end())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::PushNumber(L, it->second.result);
	FrameScript::PushNumber(L, it->second.scrapCost);
	return 2;
}

int CraftingPackets::Script_CraftingGetCatalystInfo(lua_State* L)
{
	if (FrameScript::GetTop(L) < 1 || !FrameScript::IsNumber(L, 1))
	{
		FrameScript::PushNil(L);
		return 1;
	}

	uint32_t entry = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));
	CraftingPackets& self = Instance();
	auto it = self.m_catalystLookup.find(entry);
	if (it == self.m_catalystLookup.end())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::PushNumber(L, it->second.type);
	FrameScript::PushNumber(L, it->second.value1);
	FrameScript::PushNumber(L, it->second.value2);
	return 3;
}

int CraftingPackets::Script_CraftingSetPattern(lua_State* L)
{
	CraftingPackets& self = Instance();
	uint32_t entry = 0;
	if (FrameScript::GetTop(L) >= 1 && FrameScript::IsNumber(L, 1))
		entry = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));

	bool known = false;
	for (const PatternInfo& info : self.m_patterns)
		if (info.entry == entry)
			known = true;

	self.m_pattern = known ? entry : 0;
	return 0;
}

int CraftingPackets::Script_CraftingSetMaterial(lua_State* L)
{
	CraftingPackets& self = Instance();
	uint32_t entry = 0;
	if (FrameScript::GetTop(L) >= 1 && FrameScript::IsNumber(L, 1))
		entry = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));

	bool known = false;
	for (const MaterialInfo& info : self.m_materials)
		if (info.entry == entry)
			known = true;

	self.m_material = known ? entry : 0;
	return 0;
}

int CraftingPackets::Script_CraftingGetSelection(lua_State* L)
{
	CraftingPackets& self = Instance();
	if (self.m_pattern)
		FrameScript::PushNumber(L, self.m_pattern);
	else
		FrameScript::PushNil(L);

	if (self.m_material)
		FrameScript::PushNumber(L, self.m_material);
	else
		FrameScript::PushNil(L);

	return 2;
}

int CraftingPackets::PushItemRef(lua_State* L, uint64_t guid)
{
	ItemRef ref;
	if (!ResolveItem(guid, ref))
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::PushNumber(L, ref.entry);
	FrameScript::PushNumber(L, ref.count);
	if (ref.bag >= 0)
	{
		FrameScript::PushNumber(L, ref.bag);
		FrameScript::PushNumber(L, ref.slot);
	}
	else
	{
		FrameScript::PushNil(L);
		FrameScript::PushNil(L);
	}
	return 4;
}

int CraftingPackets::Script_CraftingGetCatalyst(lua_State* L)
{
	int slot = OptionalSlotArg(L, 1, MaxCatalysts);
	if (slot < 0)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	return PushItemRef(L, Instance().m_catalysts[slot]);
}

int CraftingPackets::Script_CraftingAddCursorCatalyst(lua_State* L)
{
	int slot = OptionalSlotArg(L, 1, MaxCatalysts);
	FrameScript::PushBoolean(L, Instance().AddCursorItem(MODE_CRAFTING, slot) ? 1 : 0);
	return 1;
}

int CraftingPackets::Script_CraftingRemoveCatalyst(lua_State* L)
{
	int slot = OptionalSlotArg(L, 1, MaxCatalysts);
	if (slot < 0)
		return 0;

	CraftingPackets& self = Instance();
	if (self.m_craftPending)
	{
		SignalError(CRAFT_ERR_BUSY);
		return 0;
	}

	if (self.m_catalysts[slot])
	{
		UnlockItem(self.m_catalysts[slot]);
		self.m_catalysts[slot] = 0;
		SignalSlots(MODE_CRAFTING);
	}
	return 0;
}

int CraftingPackets::Script_CraftingGetScrapItem(lua_State* L)
{
	int slot = OptionalSlotArg(L, 1, MaxScrapItems);
	if (slot < 0)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	return PushItemRef(L, Instance().m_scrapItems[slot]);
}

int CraftingPackets::Script_CraftingGetNumScrapItems(lua_State* L)
{
	int count = 0;
	for (uint64_t guid : Instance().m_scrapItems)
		if (guid)
			++count;

	FrameScript::PushNumber(L, count);
	return 1;
}

int CraftingPackets::Script_CraftingAddCursorScrapItem(lua_State* L)
{
	int slot = OptionalSlotArg(L, 1, MaxScrapItems);
	FrameScript::PushBoolean(L, Instance().AddCursorItem(MODE_SCRAPPING, slot) ? 1 : 0);
	return 1;
}

int CraftingPackets::Script_CraftingRemoveScrapItem(lua_State* L)
{
	int slot = OptionalSlotArg(L, 1, MaxScrapItems);
	if (slot < 0)
		return 0;

	CraftingPackets& self = Instance();
	if (self.m_scrapPending)
	{
		SignalError(CRAFT_ERR_BUSY);
		return 0;
	}

	if (self.m_scrapItems[slot])
	{
		UnlockItem(self.m_scrapItems[slot]);
		self.m_scrapItems[slot] = 0;
		SignalSlots(MODE_SCRAPPING);
	}
	return 0;
}

int CraftingPackets::Script_CraftingClearSlots(lua_State* L)
{
	CraftingPackets& self = Instance();
	int mode = -1;
	if (FrameScript::GetTop(L) >= 1 && FrameScript::IsNumber(L, 1))
		mode = static_cast<int>(FrameScript::GetNumber(L, 1));

	if ((mode == -1 || mode == MODE_CRAFTING) && !self.m_craftPending)
	{
		for (uint64_t& guid : self.m_catalysts)
		{
			UnlockItem(guid);
			guid = 0;
		}
		SignalSlots(MODE_CRAFTING);
	}

	if ((mode == -1 || mode == MODE_SCRAPPING) && !self.m_scrapPending)
	{
		for (uint64_t& guid : self.m_scrapItems)
		{
			UnlockItem(guid);
			guid = 0;
		}
		SignalSlots(MODE_SCRAPPING);
	}
	return 0;
}

int CraftingPackets::Script_CraftingCraft(lua_State* L)
{
	FrameScript::PushBoolean(L, Instance().SendCraft() ? 1 : 0);
	return 1;
}

int CraftingPackets::Script_CraftingScrap(lua_State* L)
{
	FrameScript::PushBoolean(L, Instance().SendScrap() ? 1 : 0);
	return 1;
}

int CraftingPackets::Script_CraftingIsBusy(lua_State* L)
{
	CraftingPackets& self = Instance();
	FrameScript::PushBoolean(L, (self.m_craftPending || self.m_scrapPending) ? 1 : 0);
	return 1;
}

int CraftingPackets::Script_CraftingCancelPending(lua_State*)
{
	CraftingPackets& self = Instance();
	self.m_craftPending = false;
	self.m_scrapPending = false;
	return 0;
}

void CraftingPackets::RegisterLuaFunctions()
{
	sLua.RegisterFunction("CraftingIsOpen", &Script_CraftingIsOpen, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingClose", &Script_CraftingClose, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingCanCraft", &Script_CraftingCanCraft, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingGetMode", &Script_CraftingGetMode, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingSetMode", &Script_CraftingSetMode, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingGetDataVersion", &Script_CraftingGetDataVersion, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingGetScrapCurrency", &Script_CraftingGetScrapCurrency, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingGetNumPatterns", &Script_CraftingGetNumPatterns, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingGetPatternInfo", &Script_CraftingGetPatternInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingGetNumMaterials", &Script_CraftingGetNumMaterials, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingGetMaterialInfo", &Script_CraftingGetMaterialInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingGetNumRecipes", &Script_CraftingGetNumRecipes, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingGetRecipeInfo", &Script_CraftingGetRecipeInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingGetRecipe", &Script_CraftingGetRecipe, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingGetCatalystInfo", &Script_CraftingGetCatalystInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingSetPattern", &Script_CraftingSetPattern, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingSetMaterial", &Script_CraftingSetMaterial, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingGetSelection", &Script_CraftingGetSelection, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingGetCatalyst", &Script_CraftingGetCatalyst, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingAddCursorCatalyst", &Script_CraftingAddCursorCatalyst, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingRemoveCatalyst", &Script_CraftingRemoveCatalyst, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingGetScrapItem", &Script_CraftingGetScrapItem, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingGetNumScrapItems", &Script_CraftingGetNumScrapItems, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingAddCursorScrapItem", &Script_CraftingAddCursorScrapItem, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingRemoveScrapItem", &Script_CraftingRemoveScrapItem, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingClearSlots", &Script_CraftingClearSlots, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingCraft", &Script_CraftingCraft, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingScrap", &Script_CraftingScrap, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingIsBusy", &Script_CraftingIsBusy, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CraftingCancelPending", &Script_CraftingCancelPending, LuaFunctionState::FRAME);
}

void CraftingPackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_CRAFTING_DATA, &Handler_SMSG_CRAFTING_DATA);
	sCustomPacket.RegisterHandler(SMSG_CRAFTING_OPEN, &Handler_SMSG_CRAFTING_OPEN);
	sCustomPacket.RegisterHandler(SMSG_CRAFTING_CLOSE, &Handler_SMSG_CRAFTING_CLOSE);
	sCustomPacket.RegisterHandler(SMSG_CRAFTING_RESULT, &Handler_SMSG_CRAFTING_RESULT);
	sCustomPacket.RegisterHandler(SMSG_CRAFTING_SCRAP_RESULT, &Handler_SMSG_CRAFTING_SCRAP_RESULT);
}

CLIENT_DETOUR(Script_UseContainerItem, 0x005D8650, __cdecl, int, (lua_State * L))
{
	if (sCraftingPackets.HandleUseContainerItem(L))
		return 0;

	return Script_UseContainerItem(L);
}
