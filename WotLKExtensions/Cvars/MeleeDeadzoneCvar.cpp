#include "MeleeDeadzoneCvar.h"

#include <Cvars.h>
#include <Spells/AutoRepeatDeadzone.h>

namespace MeleeDeadzoneCvar
{
	void Apply()
	{
		sCvars.Register("autoRepeatMeleeDeadzone",
		    "Melee range at which auto-repeat shots stop, as a percent of the client default. 0 never stops them.", 100,
		    [](Cvar& c)
		{
			sAutoRepeatDeadzone.SetPercent(c.AsInt());
		});
	}
}
