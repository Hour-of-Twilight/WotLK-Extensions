#pragma once

#include <Defines.h>
#pragma optimize("", off)
#include "../CDBC.h"
#include "../CDBCMgr.h"

enum HDNpcDisplayFlags : uint32
{
	HD_NPC_DISPLAY_HD_RACE = 0x00000001,
	HD_NPC_DISPLAY_BAKE = 0x00000002,
	HD_NPC_DISPLAY_NPC_LOOK = 0x00000004,
	HD_NPC_DISPLAY_FACE = 0x00000008,
	HD_NPC_DISPLAY_HAIR = 0x00000010,
};

struct HDNpcDisplayRow
{
	uint32 id;
	uint32 flags;
	uint32 race;
	uint32 sex;
	uint32 retailExtraID;
	uint32 bakeTexture;
};

inline const char* g_hdNpcDisplayStrings = nullptr;
inline uint32 g_hdNpcDisplayStringsSize = 0;

class HDNpcDisplay : public CDBC
{
public:
	const char* fileName = "HDNpcDisplay";

	HDNpcDisplay() : CDBC()
	{
		this->numColumns = sizeof(HDNpcDisplayRow) / 4;
		this->rowSize = sizeof(HDNpcDisplayRow);
	}

	HDNpcDisplay* LoadDB()
	{
		GlobalCDBCMap.addCDBC(this->fileName);
		CDBC::LoadDB(this->fileName, true);
		g_hdNpcDisplayStrings = static_cast<const char*>(this->stringTable);
		g_hdNpcDisplayStringsSize = this->stringTableSize;
		HDNpcDisplay::setupTable();
		GlobalCDBCMap.setIndexRange(this->fileName, this->minIndex, this->maxIndex);
		return this;
	}

	void setupTable()
	{
		HDNpcDisplayRow* row = static_cast<HDNpcDisplayRow*>(this->rows);
		if (!row)
			return;

		for (uint32 i = 0; i < this->numRows; i++)
		{
			GlobalCDBCMap.addRow(this->fileName, row->id, *row);
			++row;
		}
	}
};
#pragma optimize("", on)
