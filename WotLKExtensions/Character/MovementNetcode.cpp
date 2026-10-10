#include <Defines.h>
#include "MovementNetcode.h"

#include <Util.h>
#include <cstdint>

namespace MovementNetcode
{
	static const uint32 kHeartbeatIntervalMs = 250;

	static const uint32 kHeartbeatIntervalDisp32[] = {
		0x006E9B97, // CMovement_C::UpdateHeartbeatTimerA, lea esi, [eax+1F4h]
		0x006F1686, // CMovement_C::SetUpdateInfo, lea ebx, [edi+1F4h]
	};

	void Apply()
	{
		for (uint32 address : kHeartbeatIntervalDisp32)
			Util::OverwriteUInt32AtAddress(address, kHeartbeatIntervalMs);
	}
}
