#pragma once

#include <Defines.h>
#pragma optimize("", off)
#include "../CDBC.h"
#include "../CDBCMgr.h"

enum HDDisplaySwapFlags : uint32
{
	HD_DISPLAY_SWAP_EVERYONE = 0x00000000,
	HD_DISPLAY_SWAP_PLAYERS_ONLY = 0x00000001,
	HD_DISPLAY_SWAP_NPCS_ONLY = 0x00000002,
};

struct HDDisplaySwapRow
{
	uint32 id;
	uint32 hdDisplayID;
	uint32 flags;
};

class HDDisplaySwap : public CDBC
{
public:
	const char* fileName = "HDDisplaySwap";

	HDDisplaySwap() : CDBC()
	{
		this->numColumns = sizeof(HDDisplaySwapRow) / 4;
		this->rowSize = sizeof(HDDisplaySwapRow);
	}

	HDDisplaySwap* LoadDB()
	{
		GlobalCDBCMap.addCDBC(this->fileName);
		GlobalCDBCMap.registerRowType<HDDisplaySwapRow>(this->fileName);
		CDBC::LoadDB(this->fileName, true);
		HDDisplaySwap::setupTable();
		GlobalCDBCMap.setIndexRange(this->fileName, this->minIndex, this->maxIndex);
		return this;
	}

	void setupTable()
	{
		HDDisplaySwapRow* row = static_cast<HDDisplaySwapRow*>(this->rows);
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
