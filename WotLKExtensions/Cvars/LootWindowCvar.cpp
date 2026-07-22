#include "LootWindowCvar.h"

#include <Cvars.h>
#include <Util.h>
#include <cstdint>

namespace LootWindowCvar
{
	static void SetStaysOpenWhileMoving(bool enabled)
	{
		static const uint32_t lootMoveCloseJz[] = {
			0x0072E64B, // CGUnit_C__OnMoveStartLocal
			0x0072E6FB, // CGUnit_C__OnStrafeStartLocal
			0x0072E7AB, // move variant (sub_72E730)
			0x0072E8BD, // CGUnit_C__OnTurnStartLocal
			0x0072E97B, // CGUnit_C__OnPitchStartLocal
			0x0072EA2B, // CGUnit_C__OnPitchStopLocal
			0x0072EB4B, // CGUnit_C__OnMovementInitiated
			0x0072EBCA, // move variant (sub_72EB80)
			0x0072AF47, // move variant
			0x0072D22E, // move variant
		};

		const uint8_t patched = enabled ? 0xEB : 0x74;
		for (uint32_t jz : lootMoveCloseJz)
			Util::SetByteAtAddress((void*)jz, patched);
	}

	void Apply()
	{
		sCvars.Register("lootStayOpenWhileMoving", "Keep the loot window open while moving.", true,
		    [](Cvar& c)
		{
			SetStaysOpenWhileMoving(c.AsBool());
		});
	}
}
