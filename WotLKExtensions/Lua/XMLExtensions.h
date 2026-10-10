#pragma once

#include <Defines.h>

#include <vector>
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include "ClientDetours.h"
#include "Logger.h"
#include "CustomLua.h"

struct Event
{
	uint32 hash;
	uint32 gap4[4];
	const char* name;
	uint32 gap18[12];
	uint32 field48;
	uint32 field4C;
	uint32 field50;
};

struct EventList
{
	size_t reserve;
	size_t size;
	Event** buf;
};

namespace RCString_C
{
	uint32 __stdcall hash(const char* str);
}

class FrameXMLExtensions
{
public:
	static void registerEvent(const char* str);
	static void LoadNewEvents();
	static void Apply();
	static int GetEventIdByName(const char* eventName);
	static bool SignalEvent(const char* eventName, const char* format = nullptr, ...);
	static const std::vector<const char*>& getCustomEvents()
	{
		return s_customEvents;
	}
	static std::vector<const char*> s_customEvents;
};

namespace GlueXML
{
	void RegisterFunctions();
	static int TestGlueLua(lua_State* L);
	static int OverrideSlotDisplay(lua_State* L);
	static void AddGlueLuaFunctions();
};
