#pragma optimize("", off)
#include <Defines.h>
#include "../CDBC.h"
#include "../CDBCMgr.h"

struct SpellClassMaskExtensionEntry
{
	uint32 spellClassSet;
	uint32 spellClassMask[3];
};

struct SpellClassMaskExtensionRow
{
	uint32 spellID;
	SpellClassMaskExtensionEntry entries[3];
};

class SpellClassMaskExtension : public CDBC
{
public:
	const char* fileName = "SpellClassMaskExtension";

	SpellClassMaskExtension() : CDBC()
	{
		this->numColumns = sizeof(SpellClassMaskExtensionRow) / 4;
		this->rowSize = sizeof(SpellClassMaskExtensionRow);
	}

	SpellClassMaskExtension* LoadDB()
	{
		GlobalCDBCMap.addCDBC(this->fileName);
		GlobalCDBCMap.registerRowType<SpellClassMaskExtensionRow>(this->fileName);
		CDBC::LoadDB(this->fileName, true);
		SpellClassMaskExtension::setupTable();
		GlobalCDBCMap.setIndexRange(this->fileName, this->minIndex, this->maxIndex);
		return this;
	}

	void setupTable()
	{
		SpellClassMaskExtensionRow* row = static_cast<SpellClassMaskExtensionRow*>(this->rows);
		if (!row)
			return;

		for (uint32 i = 0; i < this->numRows; i++)
		{
			GlobalCDBCMap.addRow(this->fileName, row->spellID, *row);
			++row;
		}
	}
};
#pragma optimize("", on)
