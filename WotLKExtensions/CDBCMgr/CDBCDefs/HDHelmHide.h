#pragma once

#include <Defines.h>
#pragma optimize("", off)
#include "../CDBC.h"
#include "../CDBCMgr.h"

constexpr uint32 HD_HELM_HIDE_EXTRA_SLOTS = 6;

struct HDHelmHideRow
{
	uint32 id;
	uint32 visID;
	uint32 race;
	uint32 maskGroups;
	uint32 groups[HD_HELM_HIDE_EXTRA_SLOTS];
};

class HDHelmHide : public CDBC
{
public:
	const char* fileName = "HDHelmHide";

	HDHelmHide() : CDBC()
	{
		this->numColumns = sizeof(HDHelmHideRow) / 4;
		this->rowSize = sizeof(HDHelmHideRow);
	}

	HDHelmHide* LoadDB()
	{
		GlobalCDBCMap.addCDBC(this->fileName);
		GlobalCDBCMap.registerRowType<HDHelmHideRow>(this->fileName);
		CDBC::LoadDB(this->fileName, true);
		HDHelmHide::setupTable();
		GlobalCDBCMap.setIndexRange(this->fileName, this->minIndex, this->maxIndex);
		return this;
	}

	void setupTable()
	{
		HDHelmHideRow* row = static_cast<HDHelmHideRow*>(this->rows);
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
