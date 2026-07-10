#include "DebugOutputCvar.h"

#include <Cvars.h>
#include <Util.h>

namespace DebugOutputCvar
{
	void Apply()
	{
		sCvars.Register("debugOutput", "Enable verbose DLL debug logging.", false,
			[](Cvar& c) { Util::SetDebugOutput(c.AsBool()); });
	}
}
