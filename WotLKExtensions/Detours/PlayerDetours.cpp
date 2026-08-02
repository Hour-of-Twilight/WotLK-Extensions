#include <ClientDetours.h>
#include <SharedDefines.h>
#include <Item.h>
#include "Logger.h"

CLIENT_DETOUR(GetComboPoints, 0x00513920, __cdecl, uint8_t, (uint64_t /*targetGuid*/))
{
	return *g_comboPointCount;
}
