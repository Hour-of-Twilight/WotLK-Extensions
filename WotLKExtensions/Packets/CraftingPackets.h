#pragma once

#include <SharedDefines.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

struct lua_State;

class CraftingPackets
{
public:
	static constexpr uint32_t MaxCatalysts = 2;
	static constexpr uint32_t MaxScrapItems = 9;

	enum Mode : uint8_t
	{
		MODE_CRAFTING = 0,
		MODE_SCRAPPING = 1
	};

	enum ErrorCode : uint8_t
	{
		CRAFT_ERR_SLOTS_FULL = 1,
		CRAFT_ERR_ALREADY_ADDED = 2,
		CRAFT_ERR_NOT_A_CATALYST = 3,
		CRAFT_ERR_CANNOT_SCRAP = 4,
		CRAFT_ERR_ITEM_LOCKED = 5,
		CRAFT_ERR_BUSY = 6
	};

	struct PatternInfo
	{
		uint32_t entry = 0;
		uint8_t category = 0;
	};

	struct MaterialInfo
	{
		uint32_t entry = 0;
		uint8_t category = 0;
	};

	struct RecipeInfo
	{
		uint32_t pattern = 0;
		uint32_t material = 0;
		uint32_t result = 0;
		uint32_t scrapCost = 0;
	};

	struct CatalystInfo
	{
		uint32_t entry = 0;
		uint8_t type = 0;
		int32_t value1 = 0;
		int32_t value2 = 0;
	};

	static CraftingPackets& Instance();

	void Apply();
	void RegisterLuaFunctions();
	void Tick();
	void Reset();

	bool IsOpen() const
	{
		return m_open;
	}

	bool HandleUseContainerItem(lua_State* L);

	CraftingPackets(const CraftingPackets&) = delete;
	CraftingPackets& operator=(const CraftingPackets&) = delete;

private:
	CraftingPackets() = default;

	struct ItemRef
	{
		uint64_t guid = 0;
		uint32_t entry = 0;
		uint32_t count = 0;
		int bag = -1;
		int slot = -1;
	};

	static void Handler_SMSG_CRAFTING_DATA(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);
	static void Handler_SMSG_CRAFTING_OPEN(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);
	static void Handler_SMSG_CRAFTING_CLOSE(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);
	static void Handler_SMSG_CRAFTING_RESULT(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);
	static void Handler_SMSG_CRAFTING_SCRAP_RESULT(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);

	static int Script_CraftingIsOpen(lua_State* L);
	static int Script_CraftingClose(lua_State* L);
	static int Script_CraftingCanCraft(lua_State* L);
	static int Script_CraftingGetMode(lua_State* L);
	static int Script_CraftingSetMode(lua_State* L);
	static int Script_CraftingGetDataVersion(lua_State* L);
	static int Script_CraftingGetScrapCurrency(lua_State* L);
	static int Script_CraftingGetNumPatterns(lua_State* L);
	static int Script_CraftingGetPatternInfo(lua_State* L);
	static int Script_CraftingGetNumMaterials(lua_State* L);
	static int Script_CraftingGetMaterialInfo(lua_State* L);
	static int Script_CraftingGetNumRecipes(lua_State* L);
	static int Script_CraftingGetRecipeInfo(lua_State* L);
	static int Script_CraftingGetRecipe(lua_State* L);
	static int Script_CraftingGetCatalystInfo(lua_State* L);
	static int Script_CraftingSetPattern(lua_State* L);
	static int Script_CraftingSetMaterial(lua_State* L);
	static int Script_CraftingGetSelection(lua_State* L);
	static int Script_CraftingGetCatalyst(lua_State* L);
	static int Script_CraftingAddCursorCatalyst(lua_State* L);
	static int Script_CraftingRemoveCatalyst(lua_State* L);
	static int Script_CraftingGetScrapItem(lua_State* L);
	static int Script_CraftingGetNumScrapItems(lua_State* L);
	static int Script_CraftingAddCursorScrapItem(lua_State* L);
	static int Script_CraftingRemoveScrapItem(lua_State* L);
	static int Script_CraftingClearSlots(lua_State* L);
	static int Script_CraftingCraft(lua_State* L);
	static int Script_CraftingScrap(lua_State* L);
	static int Script_CraftingIsBusy(lua_State* L);
	static int Script_CraftingCancelPending(lua_State* L);

	static void* ResolveContainerItem(int luaBag, int luaSlot);
	static bool ResolveItem(uint64_t guid, ItemRef& out);
	static int PushItemRef(lua_State* L, uint64_t guid);
	static bool FindItemBagSlot(uint64_t guid, int& outBag, int& outSlot);
	static bool IsItemLocked(void* item);
	static bool IsScrapCandidate(uint32_t entry);
	static void LockItem(uint64_t guid);
	static void UnlockItem(uint64_t guid);
	static void SignalError(ErrorCode code);
	static void SignalSlots(Mode mode);

	bool PlaceCatalyst(uint64_t guid, uint32_t entry, int preferredSlot);
	bool PlaceScrapItem(uint64_t guid, uint32_t entry, int preferredSlot);
	bool AddCursorItem(Mode mode, int preferredSlot);
	bool IsCatalystEntry(uint32_t entry) const;
	bool HasCatalyst(uint64_t guid) const;
	bool HasScrapItem(uint64_t guid) const;
	void UnlockAllSlots();
	void ClearSlots();
	void CloseInternal(bool notifyServer, bool signal);
	bool SendCraft();
	bool SendScrap();
	void CheckStationRange();
	void CheckPendingTimeouts();

	uint32_t m_dataVersion = 0;
	uint32_t m_scrapCurrency = 0;
	float m_interactRange = 0.f;
	std::vector<PatternInfo> m_patterns;
	std::vector<MaterialInfo> m_materials;
	std::vector<RecipeInfo> m_recipes;
	std::vector<CatalystInfo> m_catalystList;
	std::unordered_map<uint64_t, RecipeInfo> m_recipeLookup;
	std::unordered_map<uint32_t, CatalystInfo> m_catalystLookup;

	bool m_open = false;
	uint64_t m_station = 0;
	uint8_t m_flags = 0;
	Mode m_mode = MODE_CRAFTING;
	uint32_t m_pattern = 0;
	uint32_t m_material = 0;
	uint64_t m_catalysts[MaxCatalysts] = {};
	uint64_t m_scrapItems[MaxScrapItems] = {};
	bool m_craftPending = false;
	uint32_t m_craftPendingUntil = 0;
	bool m_scrapPending = false;
	uint32_t m_scrapPendingUntil = 0;
	uint32_t m_nextRangeCheck = 0;
};

#define sCraftingPackets CraftingPackets::Instance()
