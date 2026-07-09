#include <ClientDetours.h>
#include <SharedDefines.h>
#include <Item.h>
#include "Logger.h"

CLIENT_DETOUR(GetComboPointsOnTarget, 0x00513920, __cdecl, uint8_t, (uint64_t targetGuid))
{
	if (!targetGuid)
		targetGuid = *CGGameUI__m_lockedTarget;

	if (!targetGuid || targetGuid == 0xFFFFFFFFFFFFFFFE)
		return 0;

	return *g_comboPointCount;
}
