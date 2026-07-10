#pragma once

#include "SharedDefines.h"
#include <vector>

class Main;

enum class LuaFunctionState : uint8_t
{
	ALL = 0,
	FRAME = 1,
	GLUE = 2,
};

class CustomLua
{
public:
	static CustomLua& Instance();

	void RegisterFunction(const char* name, void* ptr, LuaFunctionState state = LuaFunctionState::ALL);

	static int LoadScriptFunctionsCustom();
	static void LoadGlueLuaFunctions();

private:
	CustomLua() = default;
	CustomLua(const CustomLua&) = delete;
	CustomLua& operator=(const CustomLua&) = delete;

	void Apply();
	void RegisterBuiltinFunctions();
	void LoadForState(LuaFunctionState currentState);

	struct LuaFunctionEntry
	{
		const char* name;
		void* ptr;
		LuaFunctionState state;
	};

	std::vector<LuaFunctionEntry> m_functions;

	static int FindSpellActionBarSlots(lua_State* L);
	static int ReplaceActionBarSpell(lua_State* L);
	static int SetSpellInActionBarSlot(lua_State* L);

	static int ReloadMap(lua_State* L);
	static int ToggleDisplayNormals(lua_State* L);
	static int ToggleGroundEffects(lua_State* L);
	static int ToggleLiquids(lua_State* L);
	static int ToggleM2(lua_State* L);
	static int ToggleTerrain(lua_State* L);
	static int ToggleTerrainCulling(lua_State* L);
	static int ToggleWireframeMode(lua_State* L);
	static int ToggleWMO(lua_State* L);
	static int PlayAnimation(lua_State* L);

	static int FlashGameWindow(lua_State* L);

	static int GetShapeshiftFormID(lua_State* L);
	static int GetSpellDescription(lua_State* L);
	static int GetSpellNameById(lua_State* L);
	static int GetMapNameById(lua_State* L);
	static int GetGemSpellInfo(lua_State* L);
	static int GetGemType(lua_State* L);

	static int ConvertCoordsToScreenSpace(lua_State* L);

	static int CustomLfgQueue(lua_State* L);
	static int GetSpellPen(lua_State* L);
	static int GetMagicFind(lua_State* L);
#ifdef ENABLE_DISCORD
	static int UpdateDiscordPresence(lua_State* L);
	static int ToggleDiscord(lua_State* L);
#endif
	static int GetEventId(lua_State* L);
	static int GetPlayerX(lua_State* L);
	static int GetPlayerY(lua_State* L);
	static int GetPlayerZ(lua_State* L);
	static int GetPlayerSecurityLevel(lua_State* L);
	static int SendSanityCheck(lua_State* L);
	static int GetHealthLeech(lua_State* L);
	static int GetManaLeech(lua_State* L);
	static int GetCritDamageMod(lua_State* L);
	static int GetCritHealingMod(lua_State* L);
	static int OpenUrl(lua_State* L);
	static int GetDllVersion(lua_State* L);
	static int GobEditorSelectNearest(lua_State* L);
	static int LockBagItem(lua_State* L);
	static int UnlockBagItem(lua_State* L);
	static int GetUnitItemLevel(lua_State* L);
	static int GetUnitDungeonLevel(lua_State* L);
	static int GetItemLevel(lua_State* L);
	;
	friend class Main;
};

#define sLua CustomLua::Instance()
