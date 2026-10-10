#pragma once

#include <Defines.h>

#include <cstdint>

namespace ClientData
{
	struct WoWTime
	{
		int32 minute;
		int32 hour;
		int32 weekDay;
		int32 monthDay;
		int32 month;
		int32 year;
		int32 flags;
	};

	struct WoWClientDB
	{
		void* funcTable;
		int32 isLoaded;
		int32 numRows;
		int32 maxIndex;
		int32 minIndex;
		int32 stringTable;
		void* funcTable2;
		int32* FirstRow;
		int32* Rows;
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
		uint32 ukn1;
		char* string;
	};

	struct CVar;
	typedef char(__cdecl* CVarCallback)(CVar*, const char*, const char*, const char*);
	struct CVar
	{
		uint32 pad[6];           // 0x00 b_base (TSHashObject), 0x18 bytes
		uint32 m_category;       // 0x18
		uint32 m_flags;          // 0x1C
		RCString m_stringValue;  // 0x20 (m_str at 0x28)
		float m_numberValue;     // 0x2C
		int32 m_intValue;        // 0x30
		int32 m_modified;        // 0x34
		RCString m_defaultValue; // 0x38
		RCString m_resetValue;   // 0x44
		RCString m_latchedValue; // 0x50
		RCString m_help;         // 0x5C
		CVarCallback m_callback; // 0x68
		void* m_arg;             // 0x6C
	};

	struct TerrainClickEvent
	{
		uint64 GUID;
		float x, y, z;
		uint32 button;
	};

	struct ZoneLightData
	{
		int32 mapID;
		int32 lightID;
		void* pointData;
		int32 pointNum;
		float minX;
		float minY;
		float maxX;
		float maxY;
	};
}
