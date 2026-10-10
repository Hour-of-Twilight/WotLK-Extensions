#include "LootWindowCvar.h"

#include <Cvars.h>
#include <Entities/LootWindow.h>

namespace LootWindowCvar
{
	void Apply()
	{
		sCvars.Register("lootStayOpenWhileMoving", "Keep the loot window open while moving.", true,
		    [](Cvar& c)
		{
			sLootWindow.SetStaysOpenWhileMoving(c.AsBool());
		});
	}
}
