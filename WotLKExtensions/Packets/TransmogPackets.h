#pragma once

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
		uint32_t displayId = 0;
		uint8_t inventoryType = 0;
		uint8_t sheath = 0;
		uint8_t itemClass = 0;
		uint8_t itemSubClass = 0;
		uint8_t quality = 0;
		uint32_t addedTime = 0;
		std::string name;
	};

	struct TransmogSlot
	{
		uint32_t currentDisplayId = 0;
		uint8_t currentSheath = 0;
		uint32_t originalDisplayId = 0;
		uint8_t originalSheath = 0;
		uint8_t flags = 0;
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
		uint8_t slot = 0;
		uint32_t displayId = 0;
		uint8_t inventoryType = 0;
	};

	static void Handler_SMSG_TRANSMOG_COLLECTION(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);
	static void Handler_SMSG_TRANSMOG_COLLECTION_ADD(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);
	static void Handler_SMSG_TRANSMOG_ACTIVE(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);
	static void Handler_SMSG_TRANSMOG_OPEN(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);
	static void Handler_SMSG_TRANSMOG_CLOSE(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);
	static void Handler_SMSG_TRANSMOG_RESULT(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);
	static void Handler_SMSG_TRANSMOG_COLLECTION_REMOVE(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);

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

	static uint64_t AppearanceKey(uint32_t displayId, uint8_t inventoryType);
	static void ReadAppearance(Packet& r, TransmogAppearance& out);
	static bool GetDisplayIcon(uint32_t displayId, std::string& out);
	static int PushAppearance(lua_State* L, const TransmogAppearance& appearance);
	static void* GetFrameObject(lua_State* L, int index);
	static bool IsObjectType(void* object, int typeId);
	static bool TryOnDisplay(void* frame, uint32_t displayId, uint8_t inventoryType, uint8_t sheath, int slot, const char*& reason);
	static bool TryOnArmorDisplay(void* frame, uint32_t displayId, uint8_t inventoryType, const char*& reason);
	static bool TryOnWeaponDisplay(void* frame, uint32_t displayId, uint8_t inventoryType, uint8_t sheath, int slot, const char*& reason);
	static bool RemoveHandDisplay(void* frame, void* model, int hand);

	void RebuildIndex();
	void Upsert(const TransmogAppearance& appearance);
	bool Remove(uint32_t displayId, uint8_t inventoryType);
	const TransmogAppearance* FindAppearance(uint32_t displayId, uint8_t inventoryType) const;
	void ClearOpenState();
	bool SendApply(const std::vector<ApplyEntry>& entries);

	std::vector<TransmogAppearance> m_appearances;
	std::unordered_map<uint64_t, size_t> m_index;
	std::unordered_map<uint8_t, TransmogSlot> m_slots;
	uint64_t m_npcGuid = 0;
	bool m_canApply = false;
	bool m_isOpen = false;
};

#define sTransmogPackets TransmogPackets::Instance()
