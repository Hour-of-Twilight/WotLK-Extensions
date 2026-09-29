#pragma once

#include <ClientData/DBCRows.h>

namespace HandItemDisplay
{
	enum class ModelFolder
	{
		Weapon,
		Shield
	};

	ModelFolder ResolveFolder(const ItemDisplayInfoRec* record, bool preferShield);
	int SheatheHandItem(void* unit, int slot, int moveToSheath);
}
