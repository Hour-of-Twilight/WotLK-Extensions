#include <Defines.h>
#include "TransmogPackets.h"
#include "Packet.h"

#include <ClientData/ClientFunctions.h>
#include <ClientData/GameEnums.h>
#include <CustomLua.h>
#include <CustomPacket.h>
#include <Lua/XMLExtensions.h>

#include <algorithm>
#include <cstring>

static constexpr uint32 kDressUpModelTypeIdAddress = 0x00C0E4F4;
static constexpr uint32 kMainHandPendingEntryAddress = 0x00C0E4F8;
static constexpr uint32 kOffHandPendingEntryAddress = 0x00C0E4FC;
static constexpr size_t kFrameModelOffset = 0x2A0;
static constexpr size_t kFrameComponentOffset = 0x380;
static constexpr size_t kFrameMainHandOffset = 0x384;
static constexpr size_t kFrameOffHandOffset = 0x38C;
static constexpr size_t kHandRecordSize = 8;
static constexpr size_t kHandClassOffset = 0;
static constexpr size_t kHandSubClassOffset = 1;
static constexpr size_t kHandInventoryTypeOffset = 4;
static constexpr size_t kHandSheathOffset = 5;
static constexpr uint32 kIsObjectTypeVTableSlot = 4;
static constexpr uint8 kOpenFlagCanApply = 0x01;
static constexpr uint8 kSlotFlagTransmogged = 0x01;
static constexpr int kMainHandOverride = 0x0F;
static constexpr int kOffHandOverride = 0x10;
static constexpr int kAutoHandOverride = -1;
static constexpr int kNoComponentSlot = -1;
static constexpr size_t kDisplayRecordSize = 0x64;
static constexpr size_t kDisplayIconOffset = 0x14;
static constexpr size_t kMaxApplyEntries = 255;

typedef bool(__fastcall* IsObjectTypeFn)(void* self, void* edx, int typeId);

static void Pop(lua_State* L, int n)
{
	FrameScript::SetTop(L, FrameScript::GetTop(L) - n);
}

static bool BeginMethod(lua_State* L, int objIdx, const char* method)
{
	FrameScript::GetField(L, objIdx, method);
	if (FrameScript::Type(L, -1) != ClientData::LUA_TFUNCTION)
	{
		Pop(L, 1);
		return false;
	}

	FrameScript::PushValue(L, objIdx);
	return true;
}

static void EndMethod(lua_State* L, int argCount)
{
	if (FrameScript::PCall(L, argCount + 1, 0, 0) != 0)
		Pop(L, 1);
}

static bool AppearanceLess(const TransmogPackets::TransmogAppearance& a, const TransmogPackets::TransmogAppearance& b)
{
	if (a.inventoryType != b.inventoryType)
		return a.inventoryType < b.inventoryType;

	return a.displayId < b.displayId;
}

static int ArmorComponentSlot(uint8 inventoryType)
{
	switch (inventoryType)
	{
	case ClientData::INVTYPE_HEAD:
		return 0;
	case ClientData::INVTYPE_SHOULDERS:
		return 1;
	case ClientData::INVTYPE_BODY:
		return 2;
	case ClientData::INVTYPE_CHEST:
	case ClientData::INVTYPE_ROBE:
		return 3;
	case ClientData::INVTYPE_WAIST:
		return 4;
	case ClientData::INVTYPE_LEGS:
		return 5;
	case ClientData::INVTYPE_FEET:
		return 6;
	case ClientData::INVTYPE_WRISTS:
		return 7;
	case ClientData::INVTYPE_HANDS:
		return 8;
	case ClientData::INVTYPE_TABARD:
		return 9;
	case ClientData::INVTYPE_CLOAK:
		return 10;
	default:
		return kNoComponentSlot;
	}
}

static bool IsWeaponInventoryType(uint8 inventoryType)
{
	switch (inventoryType)
	{
	case ClientData::INVTYPE_WEAPON:
	case ClientData::INVTYPE_SHIELD:
	case ClientData::INVTYPE_RANGED:
	case ClientData::INVTYPE_2HWEAPON:
	case ClientData::INVTYPE_WEAPONMAINHAND:
	case ClientData::INVTYPE_WEAPONOFFHAND:
	case ClientData::INVTYPE_HOLDABLE:
	case ClientData::INVTYPE_THROWN:
	case ClientData::INVTYPE_RANGEDRIGHT:
		return true;
	default:
		return false;
	}
}

static int ResolveHand(uint8 inventoryType, int slot)
{
	if (slot == ClientData::EQUIPMENT_SLOT_MAINHAND)
		return kMainHandOverride;
	if (slot == ClientData::EQUIPMENT_SLOT_OFFHAND)
		return kOffHandOverride;

	switch (inventoryType)
	{
	case ClientData::INVTYPE_SHIELD:
	case ClientData::INVTYPE_RANGED:
	case ClientData::INVTYPE_WEAPONOFFHAND:
	case ClientData::INVTYPE_HOLDABLE:
		return kOffHandOverride;
	default:
		return kMainHandOverride;
	}
}

static uint8* HandRecord(void* frame, int hand)
{
	return static_cast<uint8*>(frame) + (hand == kMainHandOverride ? kFrameMainHandOffset : kFrameOffHandOffset);
}

static int* HandPendingEntry(int hand)
{
	return reinterpret_cast<int*>(hand == kMainHandOverride ? kMainHandPendingEntryAddress : kOffHandPendingEntryAddress);
}

TransmogPackets& TransmogPackets::Instance()
{
	static TransmogPackets instance;
	return instance;
}

void TransmogPackets::Reset()
{
	m_appearances.clear();
	m_index.clear();
	m_slots.clear();
	ClearOpenState();
}

void TransmogPackets::ClearOpenState()
{
	m_isOpen = false;
	m_npcGuid = 0;
	m_canApply = false;
}

uint64 TransmogPackets::AppearanceKey(uint32 displayId, uint8 inventoryType)
{
	return (static_cast<uint64>(inventoryType) << 32) | displayId;
}

void TransmogPackets::RebuildIndex()
{
	m_index.clear();
	for (size_t i = 0; i < m_appearances.size(); ++i)
		m_index[AppearanceKey(m_appearances[i].displayId, m_appearances[i].inventoryType)] = i;
}

void TransmogPackets::Upsert(const TransmogAppearance& appearance)
{
	auto it = std::lower_bound(m_appearances.begin(), m_appearances.end(), appearance, AppearanceLess);
	if (it != m_appearances.end() && it->inventoryType == appearance.inventoryType && it->displayId == appearance.displayId)
		*it = appearance;
	else
		m_appearances.insert(it, appearance);

	RebuildIndex();
}

bool TransmogPackets::Remove(uint32 displayId, uint8 inventoryType)
{
	auto it = m_index.find(AppearanceKey(displayId, inventoryType));
	if (it == m_index.end() || it->second >= m_appearances.size())
		return false;

	m_appearances.erase(m_appearances.begin() + it->second);
	RebuildIndex();
	return true;
}

const TransmogPackets::TransmogAppearance* TransmogPackets::FindAppearance(uint32 displayId, uint8 inventoryType) const
{
	auto it = m_index.find(AppearanceKey(displayId, inventoryType));
	if (it == m_index.end() || it->second >= m_appearances.size())
		return nullptr;

	return &m_appearances[it->second];
}

void TransmogPackets::ReadAppearance(Packet& r, TransmogAppearance& out)
{
	out.displayId = r.GetUInt32();
	out.inventoryType = r.GetUInt8();
	out.sheath = r.GetUInt8();
	out.itemClass = r.GetUInt8();
	out.itemSubClass = r.GetUInt8();
	out.quality = r.GetUInt8();
	out.addedTime = r.GetUInt32();

	char name[256] = {};
	r.GetString(name, sizeof(name));
	out.name = name;
}

bool TransmogPackets::GetDisplayIcon(uint32 displayId, std::string& out)
{
	if (!displayId)
		return false;

	uint8 record[kDisplayRecordSize] = {};
	if (!ItemDisplayInfoDB::GetRecord(g_itemDisplayInfoDB, static_cast<int>(displayId), record))
		return false;

	const char* name = *reinterpret_cast<const char* const*>(record + kDisplayIconOffset);
	if (!name || !*name)
		return false;

	out = "Interface\\Icons\\";
	out += name;
	return true;
}

int TransmogPackets::PushAppearance(lua_State* L, const TransmogAppearance& appearance)
{
	FrameScript::PushNumber(L, appearance.displayId);
	FrameScript::PushNumber(L, appearance.inventoryType);
	FrameScript::PushNumber(L, appearance.sheath);
	FrameScript::PushNumber(L, appearance.itemClass);
	FrameScript::PushNumber(L, appearance.itemSubClass);
	FrameScript::PushNumber(L, appearance.quality);
	FrameScript::PushString(L, appearance.name.c_str());
	FrameScript::PushNumber(L, appearance.addedTime);

	std::string icon;
	if (GetDisplayIcon(appearance.displayId, icon))
		FrameScript::PushString(L, icon.c_str());
	else
		FrameScript::PushNil(L);

	return 9;
}

void* TransmogPackets::GetFrameObject(lua_State* L, int index)
{
	if (FrameScript::Type(L, index) != ClientData::LUA_TTABLE)
		return nullptr;

	FrameScript::RawGetI(L, index, 0);
	void* object = FrameScript::ToUserdata(L, -1);
	FrameScript::SetTop(L, -2);
	return object;
}

bool TransmogPackets::IsObjectType(void* object, int typeId)
{
	void** vtable = *reinterpret_cast<void***>(object);
	return reinterpret_cast<IsObjectTypeFn>(vtable[kIsObjectTypeVTableSlot])(object, nullptr, typeId);
}

bool TransmogPackets::RemoveHandDisplay(void* frame, void* model, int hand)
{
	uint8* record = HandRecord(frame, hand);
	if (!record[kHandClassOffset])
		return false;

	CCharacterComponent::RemoveHandItem(model, hand, record[kHandSheathOffset], static_cast<char>(record[kHandInventoryTypeOffset] == ClientData::INVTYPE_SHIELD));
	memset(record, 0, kHandRecordSize);
	*HandPendingEntry(hand) = 0;
	return true;
}

bool TransmogPackets::TryOnArmorDisplay(void* frame, uint32 displayId, uint8 inventoryType, const char*& reason)
{
	int componentSlot = ArmorComponentSlot(inventoryType);
	if (componentSlot == kNoComponentSlot)
	{
		reason = "invtype";
		return false;
	}

	void* component = *reinterpret_cast<void**>(static_cast<uint8*>(frame) + kFrameComponentOffset);
	if (!component)
	{
		reason = "nocomponent";
		return false;
	}

	CCharacterComponent::RemoveItem(component, componentSlot);
	CCharacterComponent::AddItemByDisplayId(component, componentSlot, static_cast<int>(displayId), 0);
	return true;
}

bool TransmogPackets::TryOnWeaponDisplay(void* frame, uint32 displayId, uint8 inventoryType, uint8 sheath, int slot, const char*& reason)
{
	void* model = *reinterpret_cast<void**>(static_cast<uint8*>(frame) + kFrameModelOffset);
	if (!model)
	{
		reason = "nomodel";
		return false;
	}

	int hand = ResolveHand(inventoryType, slot);
	int otherHand = hand == kMainHandOverride ? kOffHandOverride : kMainHandOverride;

	bool touched = RemoveHandDisplay(frame, model, hand);
	bool otherHoldsTwoHander = HandRecord(frame, otherHand)[kHandInventoryTypeOffset] == ClientData::INVTYPE_2HWEAPON;
	if (inventoryType == ClientData::INVTYPE_2HWEAPON || (hand == kOffHandOverride && otherHoldsTwoHander))
	{
		if (RemoveHandDisplay(frame, model, otherHand))
			touched = true;
	}

	uint8 displayRecord[kDisplayRecordSize] = {};
	if (!ItemDisplayInfoDB::GetRecord(g_itemDisplayInfoDB, static_cast<int>(displayId), displayRecord))
	{
		reason = touched ? "nodisplay-removed" : "nodisplay";
		return false;
	}

	uint8* record = HandRecord(frame, hand);
	record[kHandClassOffset] = (inventoryType == ClientData::INVTYPE_SHIELD || inventoryType == ClientData::INVTYPE_HOLDABLE) ? ClientData::ITEM_CLASS_ARMOR : ClientData::ITEM_CLASS_WEAPON;
	record[kHandSubClassOffset] = 0;
	record[kHandInventoryTypeOffset] = inventoryType;
	record[kHandSheathOffset] = sheath;
	*HandPendingEntry(hand) = 0;

	CCharacterComponent::AddHandItem(model, displayRecord, static_cast<unsigned int>(hand), sheath, 0,
	    static_cast<char>(inventoryType == ClientData::INVTYPE_SHIELD), static_cast<char>(inventoryType == ClientData::INVTYPE_RANGEDRIGHT), nullptr);
	return true;
}

bool TransmogPackets::TryOnDisplay(void* frame, uint32 displayId, uint8 inventoryType, uint8 sheath, int slot, const char*& reason)
{
	reason = nullptr;
	if (IsWeaponInventoryType(inventoryType))
		return TryOnWeaponDisplay(frame, displayId, inventoryType, sheath, slot, reason);

	return TryOnArmorDisplay(frame, displayId, inventoryType, reason);
}

bool TransmogPackets::SendApply(const std::vector<ApplyEntry>& entries)
{
	if (!m_isOpen || entries.empty())
		return false;

	Packet packet(CMSG_TRANSMOG_APPLY);
	packet.PutUInt8(static_cast<uint8>(entries.size()));
	for (const ApplyEntry& entry : entries)
	{
		packet.PutUInt8(entry.slot);
		packet.PutUInt32(entry.displayId);
		packet.PutUInt8(entry.inventoryType);
	}
	packet.Send();
	return true;
}

void TransmogPackets::Handler_SMSG_TRANSMOG_COLLECTION(void*, uint32, uint32, CDataStore* pkt)
{
	Packet r(pkt);
	uint32 count = r.GetUInt32();

	TransmogPackets& self = Instance();
	self.m_appearances.clear();
	for (uint32 i = 0; i < count; ++i)
	{
		TransmogAppearance appearance;
		ReadAppearance(r, appearance);
		self.m_appearances.push_back(appearance);
	}

	std::stable_sort(self.m_appearances.begin(), self.m_appearances.end(), AppearanceLess);
	self.RebuildIndex();

	FrameXMLExtensions::SignalEvent("HOT_TRANSMOG_COLLECTION_UPDATE", "");
}

void TransmogPackets::Handler_SMSG_TRANSMOG_COLLECTION_ADD(void*, uint32, uint32, CDataStore* pkt)
{
	Packet r(pkt);
	TransmogAppearance appearance;
	ReadAppearance(r, appearance);
	if (!appearance.displayId)
		return;

	Instance().Upsert(appearance);
	FrameXMLExtensions::SignalEvent("HOT_TRANSMOG_COLLECTION_ITEM_UPDATE", "%u%u", appearance.displayId, static_cast<uint32>(appearance.inventoryType));
}

void TransmogPackets::Handler_SMSG_TRANSMOG_COLLECTION_REMOVE(void*, uint32, uint32, CDataStore* pkt)
{
	Packet r(pkt);
	uint32 displayId = r.GetUInt32();
	uint8 inventoryType = r.GetUInt8();

	if (Instance().Remove(displayId, inventoryType))
		FrameXMLExtensions::SignalEvent("HOT_TRANSMOG_COLLECTION_UPDATE", "");
}

void TransmogPackets::Handler_SMSG_TRANSMOG_ACTIVE(void*, uint32, uint32, CDataStore* pkt)
{
	Packet r(pkt);
	uint8 count = r.GetUInt8();

	TransmogPackets& self = Instance();
	self.m_slots.clear();
	for (uint8 i = 0; i < count; ++i)
	{
		uint8 slot = r.GetUInt8();
		TransmogSlot info;
		info.currentDisplayId = r.GetUInt32();
		info.currentSheath = r.GetUInt8();
		info.originalDisplayId = r.GetUInt32();
		info.originalSheath = r.GetUInt8();
		info.flags = r.GetUInt8();
		self.m_slots[slot] = info;
	}

	FrameXMLExtensions::SignalEvent("HOT_TRANSMOG_ACTIVE_UPDATE", "");
}

void TransmogPackets::Handler_SMSG_TRANSMOG_OPEN(void*, uint32, uint32, CDataStore* pkt)
{
	Packet r(pkt);
	uint64 npcGuid = r.GetUInt64();
	uint8 flags = r.GetUInt8();

	TransmogPackets& self = Instance();
	self.m_npcGuid = npcGuid;
	self.m_canApply = (flags & kOpenFlagCanApply) != 0;
	self.m_isOpen = true;

	FrameXMLExtensions::SignalEvent("HOT_TRANSMOG_OPEN", "%u", self.m_canApply ? 1u : 0u);
}

void TransmogPackets::Handler_SMSG_TRANSMOG_CLOSE(void*, uint32, uint32, CDataStore*)
{
	Instance().ClearOpenState();
	FrameXMLExtensions::SignalEvent("HOT_TRANSMOG_CLOSE", "");
}

void TransmogPackets::Handler_SMSG_TRANSMOG_RESULT(void*, uint32, uint32, CDataStore* pkt)
{
	Packet r(pkt);
	uint8 code = r.GetUInt8();
	uint8 slot = r.GetUInt8();

	FrameXMLExtensions::SignalEvent("HOT_TRANSMOG_RESULT", "%u%u", static_cast<uint32>(code), static_cast<uint32>(slot));
}

int TransmogPackets::Script_TransmogRequestCollection(lua_State* L)
{
	Packet packet(CMSG_TRANSMOG_COLLECTION_REQUEST);
	if (FrameScript::GetTop(L) >= 1 && FrameScript::IsNumber(L, 1))
		packet.PutUInt8(static_cast<uint8>(FrameScript::GetNumber(L, 1)));

	packet.Send();
	return 0;
}

int TransmogPackets::Script_TransmogGetNumAppearances(lua_State* L)
{
	FrameScript::PushNumber(L, static_cast<double>(Instance().m_appearances.size()));
	return 1;
}

int TransmogPackets::Script_TransmogGetAppearanceInfo(lua_State* L)
{
	TransmogPackets& self = Instance();
	if (FrameScript::GetTop(L) < 1 || !FrameScript::IsNumber(L, 1))
	{
		FrameScript::PushNil(L);
		return 1;
	}

	int index = static_cast<int>(FrameScript::GetNumber(L, 1)) - 1;
	if (index < 0 || index >= static_cast<int>(self.m_appearances.size()))
	{
		FrameScript::PushNil(L);
		return 1;
	}

	return PushAppearance(L, self.m_appearances[index]);
}

int TransmogPackets::Script_TransmogGetAppearance(lua_State* L)
{
	if (FrameScript::GetTop(L) < 2 || !FrameScript::IsNumber(L, 1) || !FrameScript::IsNumber(L, 2))
	{
		FrameScript::PushNil(L);
		return 1;
	}

	double displayId = FrameScript::GetNumber(L, 1);
	double inventoryType = FrameScript::GetNumber(L, 2);
	if (displayId < 0 || inventoryType < 0 || inventoryType > 0xFF)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	const TransmogAppearance* appearance = Instance().FindAppearance(static_cast<uint32>(displayId), static_cast<uint8>(inventoryType));
	if (!appearance)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	return PushAppearance(L, *appearance);
}

int TransmogPackets::Script_TransmogGetSlotInfo(lua_State* L)
{
	if (FrameScript::GetTop(L) < 1 || !FrameScript::IsNumber(L, 1))
	{
		FrameScript::PushNil(L);
		return 1;
	}

	double slot = FrameScript::GetNumber(L, 1);
	if (slot < 0 || slot > 0xFF)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	TransmogPackets& self = Instance();
	auto it = self.m_slots.find(static_cast<uint8>(slot));
	if (it == self.m_slots.end())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	const TransmogSlot& info = it->second;
	FrameScript::PushNumber(L, info.currentDisplayId);
	FrameScript::PushNumber(L, info.currentSheath);
	FrameScript::PushNumber(L, info.originalDisplayId);
	FrameScript::PushNumber(L, info.originalSheath);
	FrameScript::PushBoolean(L, (info.flags & kSlotFlagTransmogged) ? 1 : 0);
	return 5;
}

int TransmogPackets::Script_TransmogIsOpen(lua_State* L)
{
	FrameScript::PushBoolean(L, Instance().m_isOpen ? 1 : 0);
	return 1;
}

int TransmogPackets::Script_TransmogGetOpenInfo(lua_State* L)
{
	FrameScript::PushBoolean(L, Instance().m_canApply ? 1 : 0);
	return 1;
}

int TransmogPackets::Script_TransmogApply(lua_State* L)
{
	std::vector<ApplyEntry> entries;
	int top = FrameScript::GetTop(L);
	for (int i = 1; i + 2 <= top; i += 3)
	{
		if (!FrameScript::IsNumber(L, i) || !FrameScript::IsNumber(L, i + 1) || !FrameScript::IsNumber(L, i + 2))
			break;

		double slot = FrameScript::GetNumber(L, i);
		double displayId = FrameScript::GetNumber(L, i + 1);
		double inventoryType = FrameScript::GetNumber(L, i + 2);
		if (slot < 0 || slot > 0xFF || displayId < 0 || inventoryType < 0 || inventoryType > 0xFF)
			continue;

		ApplyEntry entry;
		entry.slot = static_cast<uint8>(slot);
		entry.displayId = static_cast<uint32>(displayId);
		entry.inventoryType = static_cast<uint8>(inventoryType);
		entries.push_back(entry);
		if (entries.size() >= kMaxApplyEntries)
			break;
	}

	FrameScript::PushBoolean(L, Instance().SendApply(entries) ? 1 : 0);
	return 1;
}

int TransmogPackets::Script_TransmogClose(lua_State*)
{
	TransmogPackets& self = Instance();
	if (self.m_isOpen)
		Packet(CMSG_TRANSMOG_CLOSE).Send();

	self.ClearOpenState();
	return 0;
}

int TransmogPackets::Script_TransmogGetDisplayIcon(lua_State* L)
{
	std::string icon;
	if (FrameScript::GetTop(L) < 1 || !FrameScript::IsNumber(L, 1) || !GetDisplayIcon(static_cast<uint32>(FrameScript::GetNumber(L, 1)), icon))
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::PushString(L, icon.c_str());
	return 1;
}

int TransmogPackets::PushTryOnFailure(lua_State* L, const char* reason)
{
	FrameScript::PushBoolean(L, 0);
	FrameScript::PushString(L, reason ? reason : "unknown");
	return 2;
}

int TransmogPackets::Script_DressUpModelTryOnDisplay(lua_State* L)
{
	if (FrameScript::GetTop(L) < 4 || !FrameScript::IsNumber(L, 2) || !FrameScript::IsNumber(L, 3) || !FrameScript::IsNumber(L, 4))
		return PushTryOnFailure(L, "args");

	void* object = GetFrameObject(L, 1);
	if (!object)
		return PushTryOnFailure(L, "noframe");

	uint32 typeId = *reinterpret_cast<uint32*>(kDressUpModelTypeIdAddress);
	if (typeId && !IsObjectType(object, static_cast<int>(typeId)))
		return PushTryOnFailure(L, "type");

	double displayId = FrameScript::GetNumber(L, 2);
	double inventoryType = FrameScript::GetNumber(L, 3);
	double sheath = FrameScript::GetNumber(L, 4);
	if (displayId < 0 || inventoryType < 0 || inventoryType > 0xFF || sheath < 0 || sheath > 0xFF)
		return PushTryOnFailure(L, "range");

	int slot = kAutoHandOverride;
	if (FrameScript::GetTop(L) >= 5 && FrameScript::IsNumber(L, 5))
		slot = static_cast<int>(FrameScript::GetNumber(L, 5));

	const char* reason = nullptr;
	if (!TryOnDisplay(object, static_cast<uint32>(displayId), static_cast<uint8>(inventoryType), static_cast<uint8>(sheath), slot, reason))
		return PushTryOnFailure(L, reason);

	FrameScript::PushBoolean(L, 1);
	return 1;
}

int TransmogPackets::Script_DressUpModelTryOnSlot(lua_State* L)
{
	if (FrameScript::GetTop(L) < 2 || !FrameScript::IsNumber(L, 2))
	{
		FrameScript::PushBoolean(L, 0);
		return 1;
	}

	void* object = GetFrameObject(L, 1);
	if (!object)
	{
		FrameScript::PushBoolean(L, 0);
		return 1;
	}

	int itemEntry = static_cast<int>(FrameScript::GetNumber(L, 2));
	int slot = -1;
	if (FrameScript::GetTop(L) >= 3 && FrameScript::IsNumber(L, 3))
		slot = static_cast<int>(FrameScript::GetNumber(L, 3));

	uint32 typeId = *reinterpret_cast<uint32*>(kDressUpModelTypeIdAddress);
	if (!typeId)
	{
		if (!BeginMethod(L, 1, "TryOn"))
		{
			FrameScript::PushBoolean(L, 0);
			return 1;
		}

		FrameScript::PushNumber(L, itemEntry);
		EndMethod(L, 1);
		FrameScript::PushBoolean(L, 1);
		return 1;
	}

	if (!IsObjectType(object, static_cast<int>(typeId)))
	{
		FrameScript::PushBoolean(L, 0);
		return 1;
	}

	int slotOverride = kAutoHandOverride;
	if (slot == ClientData::EQUIPMENT_SLOT_MAINHAND)
		slotOverride = kMainHandOverride;
	else if (slot == ClientData::EQUIPMENT_SLOT_OFFHAND)
		slotOverride = kOffHandOverride;

	CGDressUpModelFrame::TryOn(object, itemEntry, 0, slotOverride);
	FrameScript::PushBoolean(L, 1);
	return 1;
}

void TransmogPackets::RegisterLuaFunctions()
{
	sLua.RegisterFunction("TransmogRequestCollection", &Script_TransmogRequestCollection, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TransmogGetNumAppearances", &Script_TransmogGetNumAppearances, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TransmogGetAppearanceInfo", &Script_TransmogGetAppearanceInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TransmogGetAppearance", &Script_TransmogGetAppearance, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TransmogGetSlotInfo", &Script_TransmogGetSlotInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TransmogIsOpen", &Script_TransmogIsOpen, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TransmogGetOpenInfo", &Script_TransmogGetOpenInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TransmogApply", &Script_TransmogApply, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TransmogClose", &Script_TransmogClose, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TransmogGetDisplayIcon", &Script_TransmogGetDisplayIcon, LuaFunctionState::FRAME);
	sLua.RegisterFunction("DressUpModelTryOnDisplay", &Script_DressUpModelTryOnDisplay, LuaFunctionState::FRAME);
	sLua.RegisterFunction("DressUpModelTryOnSlot", &Script_DressUpModelTryOnSlot, LuaFunctionState::FRAME);
}

void TransmogPackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_TRANSMOG_COLLECTION, &Handler_SMSG_TRANSMOG_COLLECTION);
	sCustomPacket.RegisterHandler(SMSG_TRANSMOG_COLLECTION_ADD, &Handler_SMSG_TRANSMOG_COLLECTION_ADD);
	sCustomPacket.RegisterHandler(SMSG_TRANSMOG_ACTIVE, &Handler_SMSG_TRANSMOG_ACTIVE);
	sCustomPacket.RegisterHandler(SMSG_TRANSMOG_OPEN, &Handler_SMSG_TRANSMOG_OPEN);
	sCustomPacket.RegisterHandler(SMSG_TRANSMOG_CLOSE, &Handler_SMSG_TRANSMOG_CLOSE);
	sCustomPacket.RegisterHandler(SMSG_TRANSMOG_RESULT, &Handler_SMSG_TRANSMOG_RESULT);
	sCustomPacket.RegisterHandler(SMSG_TRANSMOG_COLLECTION_REMOVE, &Handler_SMSG_TRANSMOG_COLLECTION_REMOVE);
}
