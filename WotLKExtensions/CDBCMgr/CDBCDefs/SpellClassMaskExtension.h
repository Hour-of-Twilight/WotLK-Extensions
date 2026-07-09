#pragma optimize("", off)
#include "../CDBC.h"
#include "../CDBCMgr.h"

struct SpellClassMaskExtensionEntry
{
	uint32_t spellClassSet;
	uint32_t spellClassMask[3];
};

struct SpellClassMaskExtensionRow
{
	uint32_t spellID;
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
		CDBC::LoadDB(this->fileName);
		SpellClassMaskExtension::setupTable();
		GlobalCDBCMap.setIndexRange(this->fileName, this->minIndex, this->maxIndex);
		return this;
	}

	void setupTable()
	{
		SpellClassMaskExtensionRow* row = static_cast<SpellClassMaskExtensionRow*>(this->rows);
		for (uint32_t i = 0; i < this->numRows; i++)
		{
			GlobalCDBCMap.addRow(this->fileName, row->spellID, *row);
			++row;
		}
	}
};
#pragma optimize("", on)
