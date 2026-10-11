#include <Defines.h>
#include "XMLExtensions.h"
#include "SharedDefines.h"
#include "GroupFinderPackets.h"
#include "Macros.h"
#include "Player.h"
#include <Character/HDCharacters/HDCharacters.h>
#include <Streaming/StreamLog.h>
#include <cstring>
#include <iostream>

#pragma optimize("", off)

std::vector<const char*> FrameXMLExtensions::s_customEvents;
CLIENT_FUNCTION(FireEvent_inner, 0x0081AA00, __cdecl, void, (int eventId, lua_State* L, int nargs))

void FrameXMLExtensions::registerEvent(const char* str)
{
	s_customEvents.emplace_back(str);
}

void FrameXMLExtensions::LoadNewEvents()
{
	std::vector<const char*> customFrameEvents = {
		"HOT_ITEM_CACHE_UPDATE",
		"HOT_STAT_UPDATE",
		"HOT_BAD_MPQ_NUM",
		"HOT_BAD_MPQ_FILE_INFO",
		"HOT_CHAR_SELECT_UPDATE",
		"HOT_GOB_SELECTED",
		"HOT_TALENT_CACHE",
		"HOT_TALENT_SMALL_CACHE",
		"HOT_TALENT_POINTS",
		"HOT_TALENT_LEVEL",
		"HOT_TALENT_LEARN_RESPONSE",
		"HOT_TALENT_UNLEARN_RESPONSE",
		"HOT_TALENT_RESET",
		"HOT_TALENT_INSPECT_RESPONSE",
		"HOT_TALENT_NEW",
		"HOT_TALENT_LEARNT_UPDATE",
		"HOT_TALENT_ITEM_GRANTED_UPDATE",
		"HOT_PLACE_ACTION",
		"HOT_TEST_FRAME",
		"HOT_PLAYER_ITEM_LEVEL",
		"HOT_GEM_SOCKET_UPDATE",
		"HOT_GEM_SOCKET_ERROR",
		"HOT_GEM_SOCKET_COST_UPDATE",
		"HOT_GEM_SOCKET_OPEN",
		"HOT_GEM_SOCKET_USE",
		"HOT_GEM_LOADOUTS",
		"HOT_GEM_LOADOUT_RESULT",
		"HOT_ITEM_LEVEL_UPDATE",
		"HOT_FEATURE_UPDATE",
		"HOT_STREAMING_STARTED",
		"HOT_STREAMING_STOPPED",
		"HOT_STAT_GROUPS",
		"HOT_LFG_LIST_ACTIVITIES_UPDATE",
		"HOT_LFG_LIST_ACTIVE_ENTRY_UPDATE",
		"HOT_LFG_LIST_ENTRY_RESULT",
		"HOT_LFG_LIST_SEARCH_RESULTS_RECEIVED",
		"HOT_LFG_LIST_SEARCH_RESULT_UPDATED",
		"HOT_LFG_LIST_APPLICATION_STATUS_UPDATED",
		"HOT_LFG_LIST_APPLICANT_LIST_UPDATED",
		"HOT_LFG_LIST_APPLICANT_UPDATED",
		"HOT_AURA_VALUES_UPDATED",
		"HOT_PLAYER_SETTINGS_UPDATE",
		"UNIT_ABSORB_AMOUNT_CHANGED",
		"UNIT_HEAL_ABSORB_AMOUNT_CHANGED",
		"UNIT_HEAL_PREDICTION",
		"HOT_CRAFTING_DATA_UPDATE",
		"HOT_CRAFTING_OPEN",
		"HOT_CRAFTING_CLOSE",
		"HOT_CRAFTING_SLOTS_UPDATE",
		"HOT_CRAFTING_RESULT",
		"HOT_CRAFTING_SCRAP_RESULT",
		"HOT_CRAFTING_ERROR",
		"HOT_LEGENDARIES",
		"HOT_UNIQUE_ITEMS",
		"HOT_GAME_TELES",
		"HOT_COMBO_POINT_ANCHOR",
		"HOT_TRANSMOG_COLLECTION_UPDATE",
		"HOT_TRANSMOG_COLLECTION_ITEM_UPDATE",
		"HOT_TRANSMOG_ACTIVE_UPDATE",
		"HOT_TRANSMOG_OPEN",
		"HOT_TRANSMOG_CLOSE",
		"HOT_TRANSMOG_RESULT",
		"HOT_TOWERDEFENCE_SCOREBOARD",
		"HOT_TOWERDEFENCE_SAVED_GAMES",
		"HOT_TOWERDEFENCE_MONEY",
		"HOT_TOWERDEFENCE_WAVE",
		"HOT_TOWERDEFENCE_LIVES",
		"HOT_TOWERDEFENCE_SCORE",
		"HOT_TOWERDEFENCE_GAME_END",
		"HOT_TOWERDEFENCE_TOWERS"
	};
	for (const char* eventName : customFrameEvents)
	{
		FrameXMLExtensions::registerEvent(eventName);
	}
}

void FrameXMLExtensions::Apply()
{
	FrameXMLExtensions::LoadNewEvents();
}

inline EventList* GetEventList()
{
	return (EventList*)0x00D3F7D0;
}
int FrameXMLExtensions::GetEventIdByName(const char* eventName)
{
	if (!eventName || !*eventName)
		return -1;

	EventList* eventList = GetEventList();
	if (eventList->size == 0)
		return -1;

	for (size_t i = 0; i < eventList->size; i++)
	{
		Event* event = eventList->buf[i];
		if (event && strcmp(event->name, eventName) == 0)
			return i;
	}
	return -1;
}

bool FrameXMLExtensions::SignalEvent(const char* eventName, const char* format, ...)
{
	const int eventId = GetEventIdByName(eventName);
	if (eventId < 0)
	{
		LOG_ERROR << "SignalEvent: no FrameScript event named \"" << (eventName ? eventName : "(null)")
		          << "\" in the current event table, skipping. Glue and world load different event lists, "
		             "so a world-only event cannot be fired from the glue screen.";
		return false;
	}

	va_list args;
	va_start(args, format);
	FrameScript::SignalEventV(static_cast<uint32>(eventId), format, args);
	va_end(args);
	return true;
}

namespace RCString_C
{
	inline uint32 __stdcall hash(const char* str)
	{
		return ((decltype(&hash))0x0076F640)(str);
	}
}

CLIENT_DETOUR(FrameScript_FillEvents, 0x0081B5F0, __cdecl, void, (const char** list, size_t count))
{
	LOG_DEBUG << "Loading New FrameScript Events";
	std::vector<const char*> events;
	events.reserve(count + FrameXMLExtensions::s_customEvents.size());
	events.insert(events.end(), &list[0], &list[count]);
	events.insert(events.end(), FrameXMLExtensions::s_customEvents.begin(), FrameXMLExtensions::s_customEvents.end());
	FrameScript_FillEvents(events.data(), events.size());
	LOG_DEBUG << "Loaded New FrameScript Events";
}

int GlueXML::TestGlueLua(lua_State* L)
{
	// MessageBox(NULL, "It worked!", "TestGlueLua", MB_ICONERROR | MB_OK);
	return 0;
}

static int GlueXML::OverrideSlotDisplay(lua_State* L)
{
	int slot = (int)FrameScript::GetNumber(L, 1);
	int displayId = (int)FrameScript::GetNumber(L, 2);

	void* comp = *dword_B6B1A0;
	void* model = comp ? *reinterpret_cast<void**>((uint8*)comp + 0x38) : nullptr;

	if (!comp)
	{
		LOG_DEBUG << "OverrideSlotDisplay: no character component";
		return 0;
	}

	if (!model)
	{
		LOG_DEBUG << "OverrideSlotDisplay: no base model loaded yet";
		return 0;
	}

	CCharacterComponent::AddItemByDisplayId(comp, slot, displayId, 0);
	return 0;
}

// Rebuilds the character create screen's model with the race it already has.
//
// Script_SetSelectedRace hands off to the setter below, which compares the race it is
// given against the one already selected and returns having done nothing when they match:
//
//   004E20BD  cmp edi, [0x00B6B1D0]   ; race < count
//   004E20C9  mov eax, [0x00B6B180]   ; race already selected
//   004E20CE  cmp edi, eax
//   004E20D0  je  0x4E232A            ; same race, return
//   004E20D6  ...                     ; teardown and rebuild from here
//
// That branch is why re-picking the race you already have does nothing, and so why a
// model built before a streamed art archive mounted keeps the old art until a *different*
// race is clicked. Stepping over it runs the teardown and rebuild with old == new, which
// is a forced refresh and nothing stranger: everything below keys off the same race
// either way. That is also why the global is left alone rather than given a sentinel to
// fail the comparison - the teardown indexes its arrays with the previously selected race
// and would walk off the end of them.
static int GlueXML::RebuildCharacterCreateModel(lua_State* L)
{
	constexpr uintptr_t kSetSelectedRace = 0x004E20B0; // __cdecl (int raceIndex)
	constexpr uintptr_t kSameRaceBail = 0x004E20D0;    // the je above
	constexpr uint8 kSameRaceBailBytes[] = { 0x0F, 0x84, 0x54, 0x02, 0x00, 0x00 };
	constexpr size_t kBailSize = sizeof(kSameRaceBailBytes);

	// StreamLog rather than LOG_DEBUG: the log macros compile out below LOG_LEVEL=DEBUG and
	// this needs to be readable in a shipping build, next to the streaming lines that lead here.
	auto* selectedRace = reinterpret_cast<uint32*>(0x00B6B180);
	const uint32 raceCount = *reinterpret_cast<uint32*>(0x00B6B1D0);
	if (*selectedRace >= raceCount)
	{
		Streaming::StreamLog("glue: rebuild skipped, race %u is not within %u", *selectedRace, raceCount);
		return 0;
	}

	if (std::memcmp(reinterpret_cast<void*>(kSameRaceBail), kSameRaceBailBytes, kBailSize) != 0)
	{
		// Not the branch this was written against, so leave the code alone.
		Streaming::StreamLog("glue: rebuild skipped, unexpected bytes at the same-race branch");
		return 0;
	}

	Util::OverwriteBytesAtAddress(reinterpret_cast<void*>(kSameRaceBail), 0x90, kBailSize);
	reinterpret_cast<void(__cdecl*)(uint32)>(kSetSelectedRace)(*selectedRace);
	Util::OverwriteBytesAtAddress(
	    static_cast<uint32>(kSameRaceBail), const_cast<uint8*>(kSameRaceBailBytes), kBailSize);

	Streaming::StreamLog("glue: rebuilt character create model for race %u", *selectedRace);
	return 0;
}

// Rebuilds the character select screen's model for whoever is selected.
//
// Every character record caches the character component it was built with, and the apply
// that Script_SelectCharacter runs checks that before doing anything:
//
//   004E3D5D  add  esi,eax               ; &characters[index], stride 0x198
//   004E3D61  cmp  dword [eax+0x188], 0
//   004E3D67  jne  0x4E43E3              ; already built -> skip the whole thing
//   ...
//   004E3DB1  call 0x004F0980            ; allocate the component
//   004E3DB6  mov  [esi+0x188], eax      ; and cache it on the record
//
// So once a character has been shown, re-selecting it rebuilds nothing, whatever route
// the selection takes - which is why none of the Lua paths could make new art appear.
// Clearing the cached slots puts every record back to "never built", and the apply then
// builds the selected one again against whatever art is mounted now. Returning from
// character create has the same effect, by way of repopulating the character list.
//
// The old components are dropped rather than freed: the scene may still be holding one,
// and the apply tears that down itself. It leaks a few hundred bytes per art mount,
// which is the cheaper side of a double free.
static int GlueXML::RebuildCharacterSelectModel(lua_State* L)
{
	constexpr uintptr_t kApplySelectedCharacter = 0x004E3CD0; // __cdecl ()
	constexpr uintptr_t kSelectedCharacterIndex = 0x00AC436C;
	constexpr uintptr_t kCharacterCount = 0x00B6B23C;
	constexpr uintptr_t kCharacterArray = 0x00B6B240; // -> first character record
	constexpr size_t kCharacterStride = 0x198;
	constexpr size_t kOffCachedComponent = 0x188;

	const int index = *reinterpret_cast<int*>(kSelectedCharacterIndex);
	const int count = *reinterpret_cast<int*>(kCharacterCount);
	auto* records = *reinterpret_cast<uint8**>(kCharacterArray);
	if (index < 0 || index >= count || !records)
	{
		Streaming::StreamLog("glue: select rebuild skipped, index %d of %d, records %p",
		    index,
		    count,
		    static_cast<void*>(records));
		return 0;
	}

	int cleared = 0;
	for (int i = 0; i < count; ++i)
	{
		auto** cached = reinterpret_cast<void**>(records + i * kCharacterStride + kOffCachedComponent);
		if (*cached)
		{
			*cached = nullptr;
			++cleared;
		}
	}

	reinterpret_cast<void(__cdecl*)()>(kApplySelectedCharacter)();
	Streaming::StreamLog("glue: dropped %d cached character component(s), rebuilt index %d", cleared, index);
	return 0;
}

void GlueXML::AddGlueLuaFunctions()
{
	CustomLua::LoadGlueLuaFunctions();

	// Back at character select, so anything cached for the world session is stale now.
	sGroupFinderPackets.Reset();
}

int __cdecl RegisterGlueScriptMethodsEx()
{
	int result = FrameScript::RegisterSimpleFrameScriptMethods();
	GlueXML::AddGlueLuaFunctions();
	return result;
}

void GlueXML::RegisterFunctions()
{
	sLua.RegisterFunction("TestGlueLua", TestGlueLua, LuaFunctionState::GLUE);
	sLua.RegisterFunction("OverrideSlotDisplay", OverrideSlotDisplay, LuaFunctionState::GLUE);
	sLua.RegisterFunction("RebuildCharacterCreateModel", RebuildCharacterCreateModel, LuaFunctionState::GLUE);
	sLua.RegisterFunction("RebuildCharacterSelectModel", RebuildCharacterSelectModel, LuaFunctionState::GLUE);

	Util::OverwriteUInt32AtAddress(0x4DA722, reinterpret_cast<uint32>(&RegisterGlueScriptMethodsEx) - 0x4DA726);
}
