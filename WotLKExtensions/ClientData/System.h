#pragma once

#include <cstdint>

namespace ClientData
{
	struct WoWTime
	{
		int32_t minute;
		int32_t hour;
		int32_t weekDay;
		int32_t monthDay;
		int32_t month;
		int32_t year;
		int32_t flags;
	};

	struct WoWClientDB
	{
		void* funcTable;
		int32_t isLoaded;
		int32_t numRows;
		int32_t maxIndex;
		int32_t minIndex;
		int32_t stringTable;
		void* funcTable2;
		int32_t* FirstRow;
		int32_t* Rows;
	};

	struct TSLink
	{
		TSLink* m_prevlink;
		void* m_next;
	};

	struct TSList
	{
		intptr_t m_linkoffset;
		TSLink m_terminator;
	};

	struct RCString
	{
		void** vtable;
		uint32_t ukn1;
		char* string;
	};

	struct CVar;
	typedef char(__cdecl* CVarCallback)(CVar*, const char*, const char*, const char*);
	struct CVar
	{
		uint32_t pad[6];         // 0x00 b_base (TSHashObject), 0x18 bytes
		uint32_t m_category;     // 0x18
		uint32_t m_flags;        // 0x1C
		RCString m_stringValue;  // 0x20 (m_str at 0x28)
		float m_numberValue;     // 0x2C
		int32_t m_intValue;      // 0x30
		int32_t m_modified;      // 0x34
		RCString m_defaultValue; // 0x38
		RCString m_resetValue;   // 0x44
		RCString m_latchedValue; // 0x50
		RCString m_help;         // 0x5C
		CVarCallback m_callback; // 0x68
		void* m_arg;             // 0x6C
	};

	struct TerrainClickEvent
	{
		uint64_t GUID;
		float x, y, z;
		uint32_t button;
	};

	struct ZoneLightData
	{
		int32_t mapID;
		int32_t lightID;
		void* pointData;
		int32_t pointNum;
		float minX;
		float minY;
		float maxX;
		float maxY;
	};
}
