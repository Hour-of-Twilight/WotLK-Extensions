#pragma once

#include <Defines.h>

#include <Macros.h>

#include <cstdint>

struct lua_State;

namespace ClientData::QuestLog
{
	struct Slot
	{
		uint32 questId;
		uint32 state;
		uint32 counters[2];
		uint32 timer;
	};

	struct Entry
	{
		int32 questId;
		uint32 slot;
		uint32 isHeader;
		uint32 expired;
	};

	struct Watch
	{
		uint32 questId;
		uint32 expire;
		float sortKey;
	};

	struct PoiEntry
	{
		uint8 raw[0x30];
	};

	constexpr uint32 kSlotFieldBase = 0x28;
	constexpr uint32 kSlotFieldSize = 0x14;
	constexpr uint32 kMaxWatches = 25;
	constexpr uint32 kQuestStateFailed = 0x2;
	constexpr uint32 kEventQuestLogUpdate = 0x125;
	constexpr uint32 kEventUnitQuestLogChanged = 0x1CC;
	constexpr uint32 kOpcodeQuestPoiQuery = 0x1E3;
	constexpr uint32 kOpcodeQueryTime = 0x1CE;
	constexpr uint32 kQuestRecordSortTypeOffset = 0x10;
	constexpr uint32 kCVarValueOffset = 0x30;

	CLIENT_ADDRESS(uint32, m_numQuest, 0xC23AD0)
	CLIENT_ADDRESS(uint32, m_numSortTypes, 0xC23AD4)
	CLIENT_ADDRESS(uint32, m_numWatches, 0xC23AE0)
	CLIENT_ADDRESS(int32, s_serverTimeDelta, 0xC23AE8)
	CLIENT_ADDRESS(uint8, s_trackedQuestsLoaded, 0xC23AF0)
	CLIENT_ADDRESS(uint32, s_questCallbackCount, 0xC23AF4)
	CLIENT_ADDRESS(uint32, s_questResetTime, 0xC23AF8)
	CLIENT_ADDRESS(Watch, m_watches, 0xC23680)
	CLIENT_ADDRESS(void*, s_cvQuestLogCollapseFilter, 0xBD0A34)
	CLIENT_ADDRESS(void, g_questDBCache, 0xC5DA48)

	CLIENT_FUNCTION(OnUpdateQuest, 0x6DF370, __cdecl, int, (uint32 guidLow, uint32 guidHigh, int fieldByteOffset, int unused, const Slot* oldValues))
	CLIENT_FUNCTION(GetQuestRecord, 0x67DE90, __thiscall, void*, (void* cache, uint32 questId, uint32* callbackData, void* callback, int, int))
	CLIENT_FUNCTION(QuestQueryCounterCallback, 0x5DE8F0, __cdecl, void, (int, int, int, char))
	CLIENT_FUNCTION(HasQuestPoi, 0x5E28B0, __cdecl, char, (uint32 questId))
	CLIENT_FUNCTION(SetQuestExpired, 0x5E57E0, __cdecl, int, (uint32 entryIndex))
	CLIENT_FUNCTION(QSortQuestSortTypes, 0x5DFDC0, __cdecl, int, (const void*, const void*))
	CLIENT_FUNCTION(FilterAndSortQuests, 0x5E0B80, __cdecl, uint32, ())
	CLIENT_FUNCTION(LoadTrackedQuests, 0x5E65B0, __cdecl, char, ())
	CLIENT_FUNCTION(RemoveQuestWatchById, 0x5DEA60, __cdecl, void, (uint32 questId))
	CLIENT_FUNCTION(SetCollapseFilter, 0x766940, __thiscall, char, (void* cvar, int value, char, char, char, char))
	CLIENT_FUNCTION(SendUniqueSignal, 0x615020, __cdecl, void*, (int eventId))
	CLIENT_FUNCTION(SendUnitSignal, 0x60BF10, __cdecl, void*, (const uint64* guid, int eventId))
	CLIENT_FUNCTION(GetGUIDFromName, 0x60C1C0, __cdecl, uint64, (const char* name))
	CLIENT_FUNCTION(IsPartyMember, 0x52C680, __cdecl, int, (const uint64* guid))
	CLIENT_FUNCTION(PlayerObjectPtr, 0x4D4DB0, __cdecl, void*, (uint32 guidLow, uint32 guidHigh, uint32 typeMask, const char* file, int line))

	CLIENT_FUNCTION(LuaIsNumber, 0x84DF20, __cdecl, int, (lua_State*, int))
	CLIENT_FUNCTION(LuaIsString, 0x84DF60, __cdecl, int, (lua_State*, int))
	CLIENT_FUNCTION(LuaToNumber, 0x84E030, __cdecl, double, (lua_State*, int))
	CLIENT_FUNCTION(LuaToLString, 0x84E0E0, __cdecl, const char*, (lua_State*, int, size_t*))
	CLIENT_FUNCTION(LuaPushNil, 0x84E280, __cdecl, void, (lua_State*))
	CLIENT_FUNCTION(LuaPushNumber, 0x84E2A0, __cdecl, void, (lua_State*, double))
	CLIENT_FUNCTION(LuaError, 0x84F280, __cdecl, int, (lua_State*, const char*, ...))
}
