#pragma once

#include <Defines.h>

#include <CDBCDefs/HDDisplaySwap.h>
#include <CDBCDefs/HDNpcDisplay.h>

#include <cstdint>

namespace HDCharacters::NpcDisplay
{
	const HDNpcDisplayRow* Find(uint32 displayExtraId);
	const char* BakePath(const HDNpcDisplayRow* row);
	uint32 Swap(uint32 displayId, bool isPlayer);
	bool HasSwaps();
	void Report();
}
