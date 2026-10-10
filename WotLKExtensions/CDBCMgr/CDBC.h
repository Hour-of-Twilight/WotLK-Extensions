#pragma once

#include <Defines.h>
#include <SharedDefines.h>
#include <string>

class CDBC
{
public:
	CDBC()
	{
		isLoaded = false;
		numRows = 0;
		minIndex = 0;
		maxIndex = 0;
		rows = 0;
		stringTable = 0;
		stringTableSize = 0;
	}
	uint32 numColumns;
	void* rows;
	uint32 rowSize;
	uint32 numRows;
	void* stringTable;
	uint32 stringTableSize;
	uint32 minIndex;
	uint32 maxIndex;

	CDBC* LoadDB(const char* name, bool optional = false);
	void UnloadDB();
	void GetMinMaxIndices();
	virtual ~CDBC() = default;

private:
	bool isLoaded;
};
