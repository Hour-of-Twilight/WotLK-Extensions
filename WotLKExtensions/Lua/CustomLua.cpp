#include "CustomLua.h"
#include "Player.h"
#include <string>
#include <string.h>
#ifdef ENABLE_DISCORD
#include <DiscordRPC.h>
#endif
#include <DiscordCvar.h>
#include "XMLExtensions.h"
#include <CustomPacket.h>
#include <Editor/MouseFunctions.h>
#include <Editor/EditorRuntime.h>
#include <Packets/UnitLevelCache.h>
#include <UnitDetours.h>

CustomLua& CustomLua::Instance()
{
	static CustomLua instance;
	return instance;
}

void CustomLua::RegisterFunction(const char* name, void* ptr, LuaFunctionState state)
{
	m_functions.push_back({ name, ptr, state });
}

void CustomLua::LoadForState(LuaFunctionState currentState)
{
	for (const auto& entry : m_functions)
	{
		if (entry.state == LuaFunctionState::ALL || entry.state == currentState)
			FrameScript::RegisterFunction(entry.name, entry.ptr);
	}
}

void CustomLua::Apply()
{
	RegisterBuiltinFunctions();
	Util::OverwriteUInt32AtAddress(0x52AB17, (uint32_t)&LoadScriptFunctionsCustom - 0x52AB1B);
}

int CustomLua::LoadScriptFunctionsCustom()
{
	sLua.LoadForState(LuaFunctionState::FRAME);
	return FrameScript::LoadFunctions();
}

void CustomLua::LoadGlueLuaFunctions()
{
	sLua.LoadForState(LuaFunctionState::GLUE);
}

int CustomLua::GetShapeshiftFormID(lua_State* L)
{
	uint64_t activePlayer = ClntObjMgr::GetActivePlayer();

	if (activePlayer)
	{
		CGUnit* activeObjectPtr = (CGUnit*)ClntObjMgr::ObjectPtr(activePlayer, TYPEMASK_UNIT);
		FrameScript::PushNumber(L, CGUnit_C::GetShapeshiftFormId(activeObjectPtr));
		return 1;
	}

	FrameScript::PushNumber(L, 0);
	return 1;
}

int CustomLua::GetSpellDescription(lua_State* L)
{
	if (FrameScript::IsNumber(L, 1))
	{
		uint32_t spellId = (uint32_t)FrameScript::GetNumber(L, 1);
		SpellRow row;
		char desc[1024];

		if (ClientDB::GetLocalizedRow((void*)0xAD49D0, spellId, &row))
		{
			SpellParser::ParseText(&row, &desc, 1024, 0, 0, 0, 0, 1, 0);
			FrameScript::PushString(L, desc);
			return 1;
		}
	}

	FrameScript::PushNil(L);
	return 1;
}

int CustomLua::GetSpellNameById(lua_State* L)
{
	if (FrameScript::IsNumber(L, 1))
	{
		uint32_t spellId = (uint32_t)FrameScript::GetNumber(L, 1);
		SpellRow row;

		if (ClientDB::GetLocalizedRow((void*)0xAD49D0, spellId, &row))
		{
			FrameScript::PushString(L, row.m_name_lang);
			FrameScript::PushString(L, row.m_nameSubtext_lang);
			return 2;
		}
	}

	FrameScript::PushNil(L);
	FrameScript::PushNil(L);
	return 2;
}

int CustomLua::GetMapNameById(lua_State* L)
{
	if (FrameScript::IsNumber(L, 1))
	{
		uint32_t mapId = (uint32_t)FrameScript::GetNumber(L, 1);
		MapRow* row = (MapRow*)ClientDB::GetRow((void*)0xAD4178, mapId);

		if (row && row->m_MapName_lang)
		{
			FrameScript::PushString(L, row->m_MapName_lang);
			return 1;
		}
	}

	FrameScript::PushNil(L);
	return 1;
}

namespace
{
	constexpr int ENCH_EQUIP_SPELL = 3;        // ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL
	constexpr int AURA_TEMP_LEARN_SPELL = 317; // SPELL_AURA_TEMP_LEARN_SPELL

	typedef ItemCache*(__thiscall* GetItemBlockFn)(void*, uint32_t, void*, void*, void*, int);
	typedef void*(__thiscall* GetDbRecordFn)(void*, uint32_t);

	bool ResolveGemDisplay(uint32_t itemId, uint32_t& outSpellId, uint32_t& outGemColor, const char*& outIcon)
	{
		outSpellId = 0;
		outGemColor = 0;
		outIcon = nullptr;

		auto getItemBlock = reinterpret_cast<GetItemBlockFn>(0x0067CA30); // DBItemCache_GetInfoBlockByID
		auto getRecord = reinterpret_cast<GetDbRecordFn>(0x0065C290);     // generic WowClientDB record getter

		ItemCache* item = getItemBlock(reinterpret_cast<void*>(0x00C5D828) /*WDB_CACHE_ITEM*/,
		    itemId, nullptr, nullptr, nullptr, 0);
		if (!item)
			return false; // not cached yet (a query was queued); caller should retry

		uint32_t gemPropId = static_cast<uint32_t>(item->GemProperties);
		if (!gemPropId)
			return false; // not a gem

		void* gp = getRecord(reinterpret_cast<uint8_t*>(0x00AD39A4) + 0x18 /*g_gemPropertiesDB*/, gemPropId);
		if (!gp)
			return true;
		uint32_t enchantId = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(gp) + 0x04); // m_enchantID
		outGemColor = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(gp) + 0x10);        // m_type (colour)
		if (!enchantId)
			return true;

		void* en = getRecord(reinterpret_cast<uint8_t*>(0x00AD48B0) + 0x18 /*g_spellItemEnchantmentDB*/, enchantId);
		if (!en)
			return true;
		int32_t* effect = reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(en) + 0x08);    // m_effect[3]
		int32_t* effectArg = reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(en) + 0x2C); // m_effectArg[3]
		uint32_t equipSpell = 0;
		for (int i = 0; i < 3; ++i)
			if (effect[i] == ENCH_EQUIP_SPELL && effectArg[i])
			{
				equipSpell = static_cast<uint32_t>(effectArg[i]);
				break;
			}
		if (!equipSpell)
			return true;

		uint32_t displaySpell = equipSpell;
		SpellRow row;
		if (ClientDB::GetLocalizedRow(g_SpellDB, equipSpell, &row))
		{
			for (int i = 0; i < 3; ++i)
				if (row.m_effectAura[i] == AURA_TEMP_LEARN_SPELL && row.m_effectTriggerSpell[i])
				{
					displaySpell = row.m_effectTriggerSpell[i];
					break;
				}
		}
		outSpellId = displaySpell;

		SpellRow drow;
		if (ClientDB::GetLocalizedRow(g_SpellDB, displaySpell, &drow))
		{
			auto* icon = reinterpret_cast<ClientData::SpellIconRow*>(
			    getRecord(reinterpret_cast<uint8_t*>(0x00AD488C) + 0x18 /*g_spellIconDB*/, drow.m_spellIconID));
			if (icon)
				outIcon = icon->m_textureFilename;
		}
		return true;
	}
}

int CustomLua::GetGemSpellInfo(lua_State* L)
{
	if (!FrameScript::IsNumber(L, 1))
	{
		FrameScript::PushNil(L);
		FrameScript::PushNil(L);
		FrameScript::PushNil(L);
		return 3;
	}

	uint32_t itemId = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));
	uint32_t spellId = 0;
	uint32_t gemColor = 0;
	const char* icon = nullptr;

	bool isGem = ResolveGemDisplay(itemId, spellId, gemColor, icon);

	FrameScript::PushBoolean(L, isGem);
	if (spellId)
		FrameScript::PushNumber(L, spellId);
	else
		FrameScript::PushNil(L);
	if (icon)
		FrameScript::PushString(L, icon);
	else
		FrameScript::PushNil(L);
	return 3;
}

int CustomLua::GetGemType(lua_State* L)
{
	if (!FrameScript::IsNumber(L, 1))
	{
		FrameScript::PushNil(L);
		return 1;
	}

	uint32_t itemId = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));
	uint32_t spellId = 0;
	uint32_t gemColor = 0;
	const char* icon = nullptr;

	if (!ResolveGemDisplay(itemId, spellId, gemColor, icon))
	{
		FrameScript::PushNil(L);
		return 1;
	}

	const char* typeName;
	switch (gemColor)
	{
	case 1:
		typeName = "Prismatic";
		break;
	case 2:
		typeName = "Red";
		break;
	case 4:
		typeName = "Yellow";
		break;
	case 8:
		typeName = "Blue";
		break;
	case 16:
		typeName = "Orange";
		break;
	case 32:
		typeName = "Purple";
		break;
	case 64:
		typeName = "Green";
		break;
	case 126:
		typeName = "Prismatic";
		break;
	default:
		typeName = "Gem";
		break;
	}

	FrameScript::PushString(L, typeName);
	return 1;
}

int CustomLua::FindSpellActionBarSlots(lua_State* L)
{
	uint32_t spellID = FrameScript::GetNumber(L, 1);
	uintptr_t* actionBarSpellIDs = (uintptr_t*)0xC1E358;
	uint8_t count = 0;

	for (uint8_t i = 0; i < 144; i++)
	{
		if (actionBarSpellIDs[i] == spellID)
		{
			FrameScript::PushNumber(L, i);
			count++;
		}
	}

	if (!count)
	{
		FrameScript::PushNil(L);
		return 1;
	}
	else
		return count;
}

int CustomLua::ReplaceActionBarSpell(lua_State* L)
{
	uint32_t oldSpellID = FrameScript::GetNumber(L, 1);
	uint32_t newSpellID = FrameScript::GetNumber(L, 2);
	uintptr_t* actionBarSpellIDs = (uintptr_t*)0xC1E358;
	uintptr_t* actionButtons = (uintptr_t*)0xC1DED8;

	for (uint8_t i = 0; i < 144; i++)
	{
		if (actionBarSpellIDs[i] == oldSpellID)
		{
			actionBarSpellIDs[i] = newSpellID;
			ClientPacket::MSG_SET_ACTION_BUTTON(i, 1, 0);

			for (uint8_t j = i + 72; j < 144; j += 12)
			{
				if (!actionButtons[j])
				{
					actionBarSpellIDs[i] = newSpellID;
					actionButtons[j] = 1;
					ClientPacket::MSG_SET_ACTION_BUTTON(j, 1, 0);
				}
			}
		}
	}

	return 0;
}

int CustomLua::SetSpellInActionBarSlot(lua_State* L)
{
	uint32_t spellID = FrameScript::GetNumber(L, 1);
	uint8_t slotID = FrameScript::GetNumber(L, 2);
	uintptr_t* actionBarSpellIDs = (uintptr_t*)0xC1E358;
	uintptr_t* actionButtons = (uintptr_t*)0xC1DED8;

	if (slotID < 144)
	{
		if (!actionButtons[slotID])
			actionButtons[slotID] = 1;

		actionBarSpellIDs[slotID] = spellID;
		ClientPacket::MSG_SET_ACTION_BUTTON(slotID, 1, 0);
	}

	return 0;
}

int CustomLua::ReloadMap(lua_State* L)
{
	if (!HasSecurityClearance(2))
		return 0;

	uint64_t activePlayer = ClntObjMgr::GetActivePlayer();

	if (activePlayer)
	{
		MapRow* row = 0;
		int32_t mapId = *(uint32_t*)0xBD088C;
		CGUnit* activeObjectPtr = (CGUnit*)ClntObjMgr::ObjectPtr(activePlayer, TYPEMASK_UNIT);
		MovementInfo* moveInfo = activeObjectPtr->movementInfo;

		if (mapId > -1)
		{
			row = (MapRow*)ClientDB::GetRow((void*)0xAD4178, mapId);

			if (row)
			{
				char buffer[512];

				World::UnloadMap();
				World::LoadMap(row->m_Directory, &moveInfo->position, mapId);
				SStr::Printf(buffer, 512, "Map ID: %d (Directory: \"%s\", x: %f, y: %f, z: %f) reloaded.", mapId, row->m_Directory, moveInfo->position.x, moveInfo->position.y, moveInfo->position.z);
				CGChat::AddChatMessage(buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			}
		}
	}

	return 0;
}

int CustomLua::ToggleDisplayNormals(lua_State* L)
{
	if (!HasSecurityClearance(2))
		return 0;

	char buffer[512];
	uint8_t renderFlags = *(uint8_t*)0xCD774F;
	bool areNormalsDisplayed = renderFlags & 0x40;

	if (areNormalsDisplayed)
	{
		*(uint8_t*)0xCD774F = renderFlags - 0x40;
		SStr::Printf(buffer, 512, "Normal display turned off.");
	}
	else
	{
		*(uint8_t*)0xCD774F = renderFlags + 0x40;
		SStr::Printf(buffer, 512, "Normal display turned on.");
	}

	CGChat::AddChatMessage(buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	return 0;
}

int CustomLua::ToggleGroundEffects(lua_State* L)
{
	if (!HasSecurityClearance(2))
		return 0;

	char buffer[512];
	uint8_t renderFlags = *(uint8_t*)0xCD774E;
	bool areGroundEffectsDisplayed = renderFlags & 0x10;

	if (areGroundEffectsDisplayed)
	{
		*(uint8_t*)0xCD774E = renderFlags - 0x10;
		SStr::Printf(buffer, 512, "Ground clutter hidden.");
	}
	else
	{
		*(uint8_t*)0xCD774E = renderFlags + 0x10;
		SStr::Printf(buffer, 512, "Ground clutter shown.");
	}

	CGChat::AddChatMessage(buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	return 0;
}

int CustomLua::ToggleLiquids(lua_State* L)
{
	if (!HasSecurityClearance(2))
		return 0;

	char buffer[512];
	uint8_t renderFlags = *(uint8_t*)0xCD774F;
	bool areLiquidsShowing = renderFlags & 0x3;

	if (areLiquidsShowing)
	{
		*(uint8_t*)0xCD774F = renderFlags - 0x3;
		SStr::Printf(buffer, 512, "Liquids hidden.");
	}
	else
	{
		*(uint8_t*)0xCD774F = renderFlags + 0x3;
		SStr::Printf(buffer, 512, "Liquids shown.");
	}

	CGChat::AddChatMessage(buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	return 0;
}

int CustomLua::ToggleM2(lua_State* L)
{
	if (!HasSecurityClearance(2))
		return 0;

	char buffer[512];
	uint8_t renderFlags = *(uint8_t*)0xCD774C;
	bool areM2Displayed = renderFlags & 0x1;

	if (areM2Displayed)
	{
		*(uint8_t*)0xCD774C = renderFlags - 0x1;
		SStr::Printf(buffer, 512, "Client-side M2s hidden.");
	}
	else
	{
		*(uint8_t*)0xCD774C = renderFlags + 0x1;
		SStr::Printf(buffer, 512, "Client-side M2s shown.");
	}

	CGChat::AddChatMessage(buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	return 0;
}

int CustomLua::ToggleTerrain(lua_State* L)
{
	if (!HasSecurityClearance(2))
		return 0;

	char buffer[512];
	uint8_t renderFlags = *(uint8_t*)0xCD774C;
	bool isTerrainShown = renderFlags & 0x2;

	if (isTerrainShown)
	{
		*(uint8_t*)0xCD774C = renderFlags - 0x2;
		SStr::Printf(buffer, 512, "Terrain hidden.");
	}
	else
	{
		*(uint8_t*)0xCD774C = renderFlags + 0x2;
		SStr::Printf(buffer, 512, "Terrain shown.");
	}

	CGChat::AddChatMessage(buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	return 0;
}

int CustomLua::ToggleTerrainCulling(lua_State* L)
{
	if (!HasSecurityClearance(2))
		return 0;

	char buffer[512];
	uint8_t renderFlags = *(uint8_t*)0xCD774C;
	bool isTerrainCullingOn = renderFlags & 0x32;

	if (isTerrainCullingOn)
	{
		*(uint8_t*)0xCD774C = renderFlags - 0x32;
		SStr::Printf(buffer, 512, "Terrain culling disabled.");
	}
	else
	{
		*(uint8_t*)0xCD774C = renderFlags + 0x32;
		SStr::Printf(buffer, 512, "Terrain culling enabled.");
	}

	CGChat::AddChatMessage(buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	return 0;
}

int CustomLua::ToggleWireframeMode(lua_State* L)
{
	if (!HasSecurityClearance(2))
		return 0;

	char buffer[512];
	uint8_t renderFlags = *(uint8_t*)0xCD774F;
	bool isWireframeModeOn = renderFlags & 0x20;

	if (isWireframeModeOn)
	{
		*(uint8_t*)0xCD774F = renderFlags - 0x20;
		SStr::Printf(buffer, 512, "Wireframe mode off.");
	}
	else
	{
		*(uint8_t*)0xCD774F = renderFlags + 0x20;
		SStr::Printf(buffer, 512, "Wireframe mode on.");
	}

	CGChat::AddChatMessage(buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	return 0;
}

int CustomLua::ToggleWMO(lua_State* L)
{
	if (!HasSecurityClearance(2))
		return 0;

	char buffer[512];
	uint8_t renderFlags = *(uint8_t*)0xCD774D;
	bool areWMOsDisplayed = renderFlags & 0x1;

	if (areWMOsDisplayed)
	{
		*(uint8_t*)0xCD774D = renderFlags - 0x1;
		SStr::Printf(buffer, 512, "WMOs hidden.");
	}
	else
	{
		*(uint8_t*)0xCD774D = renderFlags + 0x1;
		SStr::Printf(buffer, 512, "WMOs shown.");
	}

	CGChat::AddChatMessage(buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	return 0;
}

int CustomLua::PlayAnimation(lua_State* L)
{
	if (!HasSecurityClearance(2))
	{
		FrameScript::DisplayError(L, "You don't have the clearance for this function.");
		return 0;
	}

	if (!FrameScript::IsNumber(L, 1))
	{
		FrameScript::DisplayError(L, "Usage: PlayAnimation(animationId[, \"unit\"])");
		return 0;
	}

	int animationId = (int)FrameScript::GetNumber(L, 1);

	uint64_t guid = 0;
	const char* token = "player";
	if (FrameScript::Type(L, 2) == 4)
	{
		token = FrameScript::ToLString(L, 2, false);
		Script_GetGUIDFromToken(token, &guid, 0);
	}
	else
	{
		guid = ClntObjMgr::GetActivePlayer();
	}

	if (!guid)
	{
		FrameScript::DisplayError(L, "PlayAnimation: \"%s\" is not a valid unit.", token);
		return 0;
	}

	CGUnit* unit = (CGUnit*)ClntObjMgr::ObjectPtr(guid, TYPEMASK_UNIT);
	if (!unit)
	{
		FrameScript::DisplayError(L, "PlayAnimation: \"%s\" is not in range / not loaded.", token);
		return 0;
	}

	CGUnit_C::AnimationData(unit, animationId, 0);

	char buffer[256];
	SStr::Printf(buffer, 256, "Playing animation %d on %s.", animationId, token);
	CGChat::AddChatMessage(buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	return 0;
}

int CustomLua::FlashGameWindow(lua_State* L)
{
	HWND activeWindow = *(HWND*)0x00D41620;

	if (activeWindow && GetForegroundWindow() != activeWindow)
	{
		FLASHWINFO flashInfo;

		flashInfo.cbSize = sizeof(flashInfo);
		flashInfo.hwnd = activeWindow;
		flashInfo.dwFlags = FLASHW_TIMERNOFG | FLASHW_TRAY;
		flashInfo.uCount = -1;
		flashInfo.dwTimeout = 500;

		FlashWindowEx(&flashInfo);
	}

	return 0;
}

int CustomLua::ConvertCoordsToScreenSpace(lua_State* L)
{
	float ox = FrameScript::GetNumber(L, 1);
	float oy = FrameScript::GetNumber(L, 2);
	float oz = FrameScript::GetNumber(L, 3);
	void* worldFrame = *(void**)0x00B7436C;
	C3Vector pos3d = { ox, oy, oz };
	C3Vector pos2d = {};
	uint32_t flags = 0;
	int result = World::Pos3Dto2D(worldFrame, nullptr, &pos3d, &pos2d, &flags);
	float x;
	float y;

	Util::PercToScreenPos(pos2d.x, pos2d.y, &x, &y);
	FrameScript::PushNumber(L, x);
	FrameScript::PushNumber(L, y);
	FrameScript::PushNumber(L, pos2d.z);
	return 3;
}

int CustomLua::CustomLfgQueue(lua_State* L)
{
	uint32 role = (uint32)FrameScript::GetNumber(L, 1);
	char* selections = FrameScript::ToLString(L, 2, false);
	bool premadeWait = FrameScript::GetNumber(L, 3) == 1;
	uint32 affixes[MAX_AFFIXES];
	for (uint8 i = 0; i < MAX_AFFIXES; ++i)
	{
		affixes[i] = FrameScript::GetNumber(L, 4 + i);
	}

	CDataStore pkt;

	CDataStore_C::GenPacket(&pkt);
	CDataStore_C::PutInt32(&pkt, CMSG_CUSTOM_LFG_QUEUE);
	CDataStore_C::PutInt32(&pkt, role);
	CDataStore_C::PutInt8(&pkt, premadeWait);
	CDataStore_C::PutInt32(&pkt, affixes[0]);
	CDataStore_C::PutInt32(&pkt, affixes[1]);
	CDataStore_C::PutInt32(&pkt, affixes[2]);
	CDataStore_C::PutInt32(&pkt, affixes[3]);
	CDataStore_C::PutCString(&pkt, selections);

	// for (uint8 i = 0; i < MAX_AFFIXES; ++i)
	// CDataStore_C::PutInt32(&pkt, affixes[i]);

	pkt.m_read = 0;
	// /script CustomLfgQueue(15,"307",0,1,2,3,4)
	ClientServices::SendPacket(&pkt);
	CDataStore_C::Release(&pkt);
	return 0;
}

int CustomLua::GetSpellPen(lua_State* L)
{
	uint8 school = FrameScript::GetNumber(L, 1);
	if (school >= MAX_SPELL_SCHOOL)
	{
		FrameScript::DisplayError(L, "%u is not a valid school", school);
		return 0;
	}

	FrameScript::PushNumber(L, sPlayer.GetCustomSpellPen(school));
	return 1;
}

int CustomLua::GetMagicFind(lua_State* L)
{
	FrameScript::PushNumber(L, sPlayer.GetMagicFind());
	return 1;
}

#ifdef ENABLE_DISCORD
int CustomLua::UpdateDiscordPresence(lua_State* L)
{
	char* status = FrameScript::ToLString(L, 1, false);
	char* details = FrameScript::ToLString(L, 2, false);
	// char* icon = FrameScript::ToLString(L, 3, false);
	//  in future, allow custom icons per instance?
	sDiscord.UpdateActivity(details, status, /*icon*/ "icon_transparent_");
	return 0;
}

int CustomLua::ToggleDiscord(lua_State* L)
{
	bool enabled = FrameScript::GetNumber(L, 1) == 1;
	// The enableDiscord cvar's handler does the actual init/shutdown and persists.
	DiscordCvar::SetEnabled(enabled);
	return 0;
}
#endif

int CustomLua::GetEventId(lua_State* L)
{
	if (!HasSecurityClearance(2))
	{
		FrameScript::DisplayError(L, "You don't have the clearance for this function.");
		return 0;
	}

	char* eventName = FrameScript::ToLString(L, 1, false);
	int32_t eventId = FrameXMLExtensions::GetEventIdByName(eventName);
	FrameScript::PushNumber(L, eventId);
	return 1;
}

int CustomLua::GetPlayerX(lua_State* L)
{
	if (!HasSecurityClearance(2))
	{
		FrameScript::DisplayError(L, "You don't have the clearance for this function.");
		return 0;
	}
	CGPlayer* activeObjectPtr = ClntObjMgr::GetActivePlayerObj();
	if (activeObjectPtr)
	{
		C3Vector position = activeObjectPtr->unitBase.movementInfo->position;
		FrameScript::PushNumber(L, position.x);
		return 1;
	}
	return 0;
}

int CustomLua::GetPlayerY(lua_State* L)
{
	if (!HasSecurityClearance(2))
	{
		FrameScript::DisplayError(L, "You don't have the clearance for this function.");
		return 0;
	}
	CGPlayer* activeObjectPtr = ClntObjMgr::GetActivePlayerObj();
	if (activeObjectPtr)
	{
		C3Vector position = activeObjectPtr->unitBase.movementInfo->position;
		FrameScript::PushNumber(L, position.y);
		return 1;
	}
	return 0;
}

int CustomLua::GetPlayerZ(lua_State* L)
{
	if (!HasSecurityClearance(2))
	{
		FrameScript::DisplayError(L, "You don't have the clearance for this function.");
		return 0;
	}
	CGPlayer* activeObjectPtr = ClntObjMgr::GetActivePlayerObj();
	if (activeObjectPtr)
	{
		C3Vector position = activeObjectPtr->unitBase.movementInfo->position;
		FrameScript::PushNumber(L, position.z);
		return 1;
	}
	return 0;
}

int CustomLua::SendSanityCheck(lua_State* L)
{
	sCustomPacket.SendSanityCheck(true);
	return 0;
}

int CustomLua::GetHealthLeech(lua_State* L)
{
	FrameScript::PushNumber(L, sPlayer.GetHealthLeech());
	return 1;
}

int CustomLua::GetManaLeech(lua_State* L)
{
	FrameScript::PushNumber(L, sPlayer.GetManaLeech());
	return 1;
}

int CustomLua::GetCritDamageMod(lua_State* L)
{
	FrameScript::PushNumber(L, sPlayer.GetCritDamageMod());
	return 1;
}

int CustomLua::GetCritHealingMod(lua_State* L)
{
	FrameScript::PushNumber(L, sPlayer.GetCritHealingMod());
	return 1;
}

int CustomLua::OpenUrl(lua_State* L)
{
	const char* url = FrameScript::ToLString(L, 1, false);
	if (!url || strlen(url) == 0)
		return 0;

	const char* domain = "houroftwilight.net";
	const char* httpPrefix = "http://houroftwilight.net";
	const char* httpsPrefix = "https://houroftwilight.net";

	bool validDomain = false;

	if (strncmp(url, httpPrefix, strlen(httpPrefix)) == 0 ||
	    strncmp(url, httpsPrefix, strlen(httpsPrefix)) == 0)
	{
		size_t prefixLen = strncmp(url, httpsPrefix, strlen(httpsPrefix)) == 0
		                       ? strlen(httpsPrefix)
		                       : strlen(httpPrefix);

		char next = url[prefixLen];

		if (next == '/' || next == '?' || next == '#' || next == '\0')
			validDomain = true;
	}

	if (!validDomain)
		return 0;

	const char* blacklist = "<>\"'`|&;${}()[]\\";
	if (strpbrk(url, blacklist) != nullptr)
		return 0;

	ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
	return 0;
}

int CustomLua::GetDllVersion(lua_State* L)
{
	FrameScript::PushNumber(L, DLL_VER);
	return 1;
}

int CustomLua::GobEditorSelectNearest(lua_State* L)
{
	if (!HasSecurityClearance(3))
		return 0;
	sCustomPacket.SendGobEditorSelectNearest();
	return 0;
}

int CustomLua::GetPlayerSecurityLevel(lua_State* L)
{
	FrameScript::PushNumber(L, sPlayer.GetSecurityLevel());
	return 1;
}

static void* GetItemFromLuaArg(lua_State* L)
{
	int argc = FrameScript::GetTop(L);
	Util::DebugOutput("Getting item from Lua args");

	if (argc == 0 || FrameScript::Type(L, 1) == 0 /*LUA_TNIL*/)
	{
		Util::DebugOutput("Getting cursor item");
		uint64_t guid = CGGameUI::GetCursorItem();
		if (!guid)
		{
			Util::DebugOutput("No cursor item found");
			return nullptr;
		}
		return ClntObjMgr::ObjectPtr(guid, 0x2);
	}

	if (argc >= 2 && FrameScript::IsNumber(L, 1) && FrameScript::IsNumber(L, 2))
	{
		Util::DebugOutput("ASM Calling getluabagslot");
		void* bagObj = nullptr;
		int slot = 0;
		void* unused = nullptr;
		int ok = 0;
		void* fn = (void*)0x005D7380; // CGContainerInfo__GetLuaBagAndSlot

		__asm
		{
			lea  eax, slot
			lea  ecx, unused
			push ecx
			lea  ecx, bagObj
			push ecx
			push L
			call fn
			add  esp, 12
			mov  ok, eax
		}

		if (!ok || !bagObj)
		{
			Util::DebugOutput("Invalid bag/slot");
			return nullptr;
		}

		return CGBag_C::GetItemPointer(bagObj, slot);
	}

	uint32_t entryId = 0;
	if (FrameScript::IsNumber(L, 1))
	{
		entryId = (uint32_t)FrameScript::GetNumber(L, 1);
	}
	else
	{
		const char* name = FrameScript::ToLString(L, 1, false);
		if (!name || name[0] == '\0')
			return nullptr;
		entryId = CGItem_C::GetItemByFullName(name);
	}

	if (!entryId)
		return nullptr;

	uint64_t playerGuid = ClntObjMgr::GetActivePlayer();
	void* player = ClntObjMgr::ObjectPtr(playerGuid, 0x10);
	if (!player)
		return nullptr;

	void* bags = reinterpret_cast<uint8_t*>(player) + 0x18F0;
	return CGBag_C::FindItemOfType(bags, entryId, 0xA1);
}

int CustomLua::LockBagItem(lua_State* L)
{
	void* item = GetItemFromLuaArg(L);
	if (item)
	{
		Util::DebugOutput("Locking item");
		CGItem_C::Lock(item);
	}
	else
		Util::DebugOutput("No item?");
	return 0;
}

int CustomLua::UnlockBagItem(lua_State* L)
{
	void* item = GetItemFromLuaArg(L);
	if (item)
		CGItem_C::Unlock(item);
	return 0;
}

int CustomLua::GetUnitItemLevel(lua_State* L)
{
	const char* token = FrameScript::ToLString(L, 1, false);
	if (!token)
	{
		FrameScript::PushNil(L);
		return 1;
	}
	uint64_t guid = 0;
	Script_GetGUIDFromToken(token, &guid, 0);
	if (!guid)
	{
		FrameScript::PushNil(L);
		return 1;
	}
	if (!sUnitLevelCache.HasPlayerItemLevel(guid))
	{
		UnitLevelCache::SendRequest(guid);
		FrameScript::PushNil(L);
		return 1;
	}
	FrameScript::PushNumber(L, sUnitLevelCache.GetPlayerItemLevel(guid));
	return 1;
}

int CustomLua::GetUnitDungeonLevel(lua_State* L)
{
	const char* token = FrameScript::ToLString(L, 1, false);
	if (!token)
	{
		FrameScript::PushNil(L);
		return 1;
	}
	uint64_t guid = 0;
	Script_GetGUIDFromToken(token, &guid, 0);
	if (!guid)
	{
		FrameScript::PushNil(L);
		return 1;
	}
	if (!sUnitLevelCache.HasUnitItemLevelOrDungeonLevel(guid))
	{
		UnitLevelCache::SendRequest(guid);
		FrameScript::PushNil(L);
		return 1;
	}
	FrameScript::PushNumber(L, sUnitLevelCache.GetUnitItemLevelOrDungeonLevel(guid));
	return 1;
}

int CustomLua::GetItemLevel(lua_State* L)
{
	uint64 ourGuid = ClntObjMgr::GetActivePlayer();
	if (!sUnitLevelCache.HasUnitItemLevelOrDungeonLevel(ourGuid))
	{
		UnitLevelCache::SendRequest(ourGuid);
		FrameScript::PushNil(L);
		return 1;
	}
	uint32 itemLevel = sUnitLevelCache.GetPlayerItemLevel(ourGuid);
	FrameScript::PushNumber(L, itemLevel);
	return 1;
}

void CustomLua::RegisterBuiltinFunctions()
{
	if (outOfBoundLuaFunctions)
	{
		RegisterFunction("FlashGameWindow", &FlashGameWindow, LuaFunctionState::ALL);
		RegisterFunction("GetShapeshiftFormID", &GetShapeshiftFormID, LuaFunctionState::FRAME);
		RegisterFunction("GetSpellDescription", &GetSpellDescription, LuaFunctionState::FRAME);
		RegisterFunction("GetSpellNameById", &GetSpellNameById, LuaFunctionState::FRAME);
		RegisterFunction("GetMapNameById", &GetMapNameById, LuaFunctionState::FRAME);
		RegisterFunction("GetGemSpellInfo", &GetGemSpellInfo, LuaFunctionState::FRAME);
		RegisterFunction("GetGemType", &GetGemType, LuaFunctionState::FRAME);
		RegisterFunction("ConvertCoordsToScreenSpace", &ConvertCoordsToScreenSpace, LuaFunctionState::FRAME);
	}

	if (customActionBarFunctions)
	{
		RegisterFunction("FindSpellActionBarSlots", &FindSpellActionBarSlots, LuaFunctionState::FRAME);
		RegisterFunction("ReplaceActionBarSpell", &ReplaceActionBarSpell, LuaFunctionState::FRAME);
		RegisterFunction("SetSpellInActionBarSlot", &SetSpellInActionBarSlot, LuaFunctionState::FRAME);
	}

	if (devHelperFunctions)
	{
		RegisterFunction("ReloadMap", &ReloadMap, LuaFunctionState::FRAME);
		RegisterFunction("ToggleDisplayNormals", &ToggleDisplayNormals, LuaFunctionState::FRAME);
		RegisterFunction("ToggleGroundEffects", &ToggleGroundEffects, LuaFunctionState::FRAME);
		RegisterFunction("ToggleM2", &ToggleM2, LuaFunctionState::FRAME);
		RegisterFunction("ToggleLiquids", &ToggleLiquids, LuaFunctionState::FRAME);
		RegisterFunction("ToggleTerrain", &ToggleTerrain, LuaFunctionState::FRAME);
		RegisterFunction("ToggleTerrainCulling", &ToggleTerrainCulling, LuaFunctionState::FRAME);
		RegisterFunction("ToggleWireframeMode", &ToggleWireframeMode, LuaFunctionState::FRAME);
		RegisterFunction("ToggleWMO", &ToggleWMO, LuaFunctionState::FRAME);
		RegisterFunction("PlayAnimation", &PlayAnimation, LuaFunctionState::FRAME);
		RegisterFunction("GetEventId", &GetEventId, LuaFunctionState::FRAME);
		RegisterFunction("GobEditorSelectNearest", &GobEditorSelectNearest, LuaFunctionState::FRAME);
	}

	if (customPackets)
	{
		RegisterFunction("CustomLfgQueue", &CustomLfgQueue, LuaFunctionState::FRAME);
		RegisterFunction("GetSpellPen", &GetSpellPen, LuaFunctionState::FRAME);
		RegisterFunction("GetMagicFind", &GetMagicFind, LuaFunctionState::FRAME);
		// RegisterFunction("PortGraveyard", &PortGraveyard, LuaFunctionState::FRAME);
	}
#ifdef ENABLE_DISCORD
	RegisterFunction("UpdateDiscordPresence", &UpdateDiscordPresence, LuaFunctionState::FRAME);
	RegisterFunction("ToggleDiscord", &ToggleDiscord, LuaFunctionState::ALL);
#endif
	RegisterFunction("GetPlayerX", &GetPlayerX, LuaFunctionState::FRAME);
	RegisterFunction("GetPlayerY", &GetPlayerY, LuaFunctionState::FRAME);
	RegisterFunction("GetPlayerZ", &GetPlayerZ, LuaFunctionState::FRAME);
	RegisterFunction("GetPlayerSecurityLevel", &GetPlayerSecurityLevel, LuaFunctionState::FRAME);
	RegisterFunction("GetHealthLeech", &GetHealthLeech, LuaFunctionState::FRAME);
	RegisterFunction("GetManaLeech", &GetManaLeech, LuaFunctionState::FRAME);
	RegisterFunction("GetCritDamageMod", &GetCritDamageMod, LuaFunctionState::FRAME);
	RegisterFunction("GetCritHealingMod", &GetCritHealingMod, LuaFunctionState::FRAME);
	RegisterFunction("OpenUrl", &OpenUrl, LuaFunctionState::ALL);
	RegisterFunction("GetDllVersion", &GetDllVersion, LuaFunctionState::ALL);
	sPlayer.RegisterCustomLuaFunctions();

	RegisterFunction("GetMouseWorldPosition", &GetMouseWorldPosition, LuaFunctionState::FRAME);
	RegisterFunction("GetLastMouseoverGUID", &GetLastMouseoverGUID, LuaFunctionState::FRAME);
	RegisterFunction("LogMouseoverGobValues", &LogMouseoverGobValues, LuaFunctionState::FRAME);
	RegisterFunction("SelectEditorGobByMouse", &SelectEditorGobByMouse, LuaFunctionState::FRAME);
	RegisterFunction("GetSelectedGobGUID", &GetSelectedGobGUID, LuaFunctionState::FRAME);
	RegisterFunction("GetSelectedGobPosition", &GetSelectedGobPosition, LuaFunctionState::FRAME);
	RegisterFunction("GetSelectedGobRotation", &GetSelectedGobRotation, LuaFunctionState::FRAME);
	RegisterFunction("SetGobPositionByGUID", &SetGobPositionByGUID, LuaFunctionState::FRAME);
	RegisterFunction("SetGobRotationByGUID", &SetGobRotationByGUID, LuaFunctionState::FRAME);
	RegisterFunction("ClearSelectedGob", &ClearSelectedGob, LuaFunctionState::FRAME);
	RegisterFunction("CommitEditorGobToServer", &CommitEditorGobToServer, LuaFunctionState::FRAME);
	RegisterFunction("LockBagItem", &LockBagItem, LuaFunctionState::FRAME);
	RegisterFunction("UnlockBagItem", &UnlockBagItem, LuaFunctionState::FRAME);
	RegisterFunction("GetUnitItemLevel", &GetUnitItemLevel, LuaFunctionState::FRAME);
	RegisterFunction("GetUnitDungeonLevel", &GetUnitDungeonLevel, LuaFunctionState::FRAME);
	RegisterFunction("GetOurItemLevel", &GetItemLevel, LuaFunctionState::FRAME);
	RegisterFunction("TrueLevel", &Script_TrueLevel, LuaFunctionState::FRAME);
}
