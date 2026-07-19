#include "ActionRepeatCvar.h"

#include <Cvars.h>
#include <Input/ActionRepeat.h>

namespace ActionRepeatCvar
{
	void Apply()
	{
		sCvars.Register("actionButtonHoldRepeat",
		    "Auto-repeat a held action bar keybind. 0 off, 1 until it casts, 2 continuous.", 0,
		    [](Cvar& c)
		{
			sActionRepeat.SetMode(c.AsInt());
		});

		sCvars.Register("actionButtonHoldRepeatDelay",
		    "Grace period in milliseconds before a held action bar keybind starts repeating.", 500,
		    [](Cvar& c)
		{
			sActionRepeat.SetDelayMs(c.AsInt());
		});

		sCvars.Register("actionButtonHoldRepeatInterval",
		    "Milliseconds between repeats of a held action bar keybind.", 100,
		    [](Cvar& c)
		{
			sActionRepeat.SetIntervalMs(c.AsInt());
		});
	}
}
