#pragma once

#include <Defines.h>

#include <SharedDefines.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct lua_State;
class Packet;

class TransmogPackets
{
public:
	struct TransmogAppearance
	{
		uint32 displayId = 0;
		uint8 inventoryType = 0;
		uint8 sheath = 0;
		uint8 itemClass = 0;
		uint8 itemSubClass = 0;
		uint8 quality = 0;
		uint32 addedTime = 0;
		std::string name;
	};

	struct TransmogSlot
	{
		uint32 currentDisplayId = 0;
		uint8 currentSheath = 0;
		uint32 originalDisplayId = 0;
		uint8 originalSheath = 0;
		uint8 flags = 0;
	};

	static TransmogPackets& Instance();

	void Apply();
	void RegisterLuaFunctions();
	void Reset();

	bool IsOpen() const
	{
		return m_isOpen;
	}

	TransmogPackets(const TransmogPackets&) = delete;
	TransmogPackets& operator=(const TransmogPackets&) = delete;

private:
	TransmogPackets() = default;

	struct ApplyEntry
	{
		uint8 slot = 0;
		uint32 displayId = 0;
		uint8 inventoryType = 0;
	};

	static void Handler_SMSG_TRANSMOG_COLLECTION(void* param, uint32 opcode, uint32 a2, CDataStore* pkt);
	static void Handler_SMSG_TRANSMOG_COLLECTION_ADD(void* param, uint32 opcode, uint32 a2, CDataStore* pkt);
	static void Handler_SMSG_TRANSMOG_ACTIVE(void* param, uint32 opcode, uint32 a2, CDataStore* pkt);
	static void Handler_SMSG_TRANSMOG_OPEN(void* param, uint32 opcode, uint32 a2, CDataStore* pkt);
	static void Handler_SMSG_TRANSMOG_CLOSE(void* param, uint32 opcode, uint32 a2, CDataStore* pkt);
	static void Handler_SMSG_TRANSMOG_RESULT(void* param, uint32 opcode, uint32 a2, CDataStore* pkt);
	static void Handler_SMSG_TRANSMOG_COLLECTION_REMOVE(void* param, uint32 opcode, uint32 a2, CDataStore* pkt);

	static int Script_TransmogRequestCollection(lua_State* L);
	static int Script_TransmogGetNumAppearances(lua_State* L);
	static int Script_TransmogGetAppearanceInfo(lua_State* L);
	static int Script_TransmogGetAppearance(lua_State* L);
	static int Script_TransmogGetSlotInfo(lua_State* L);
	static int Script_TransmogIsOpen(lua_State* L);
	static int Script_TransmogGetOpenInfo(lua_State* L);
	static int Script_TransmogApply(lua_State* L);
	static int Script_TransmogClose(lua_State* L);
	static int Script_TransmogGetDisplayIcon(lua_State* L);
	static int Script_DressUpModelTryOnDisplay(lua_State* L);
	static int PushTryOnFailure(lua_State* L, const char* reason);
	static int Script_DressUpModelTryOnSlot(lua_State* L);

	static uint64 AppearanceKey(uint32 displayId, uint8 inventoryType);
	static void ReadAppearance(Packet& r, TransmogAppearance& out);
	static bool GetDisplayIcon(uint32 displayId, std::string& out);
	static int PushAppearance(lua_State* L, const TransmogAppearance& appearance);
	static void* GetFrameObject(lua_State* L, int index);
	static bool IsObjectType(void* object, int typeId);
	static bool TryOnDisplay(void* frame, uint32 displayId, uint8 inventoryType, uint8 sheath, int slot, const char*& reason);
	static bool TryOnArmorDisplay(void* frame, uint32 displayId, uint8 inventoryType, const char*& reason);
	static bool TryOnWeaponDisplay(void* frame, uint32 displayId, uint8 inventoryType, uint8 sheath, int slot, const char*& reason);
	static bool RemoveHandDisplay(void* frame, void* model, int hand);

	void RebuildIndex();
	void Upsert(const TransmogAppearance& appearance);
	bool Remove(uint32 displayId, uint8 inventoryType);
	const TransmogAppearance* FindAppearance(uint32 displayId, uint8 inventoryType) const;
	void ClearOpenState();
	bool SendApply(const std::vector<ApplyEntry>& entries);

	std::vector<TransmogAppearance> m_appearances;
	std::unordered_map<uint64, size_t> m_index;
	std::unordered_map<uint8, TransmogSlot> m_slots;
	uint64 m_npcGuid = 0;
	bool m_canApply = false;
	bool m_isOpen = false;
};

#define sTransmogPackets TransmogPackets::Instance()
