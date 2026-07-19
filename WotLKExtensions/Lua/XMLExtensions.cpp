#include "XMLExtensions.h"
#include "SharedDefines.h"
#include "Macros.h"
#include "Player.h"
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
		"HOT_ITEM_LEVEL_UPDATE",
		"HOT_FEATURE_UPDATE",
		"HOT_STREAMING_STARTED",
		"HOT_STREAMING_STOPPED"
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

namespace RCString_C
{
	inline uint32_t __stdcall hash(const char* str)
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

void GlueXML::AddGlueLuaFunctions()
{
	CustomLua::LoadGlueLuaFunctions();
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

	Util::OverwriteUInt32AtAddress(0x4DA722, reinterpret_cast<uint32_t>(&RegisterGlueScriptMethodsEx) - 0x4DA726);
}
