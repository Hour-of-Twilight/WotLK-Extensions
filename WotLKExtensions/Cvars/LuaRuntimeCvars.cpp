#include "LuaRuntimeCvars.h"

#include <Cvars.h>
#include <Lua/LuaRuntime.h>

namespace LuaRuntimeCvars
{
	void Apply()
	{
		sCvars.Register("luaPerfStats",
		    "Collect per-frame UI timings: Lua handler time and calls, GC time, allocation and heap size (GetLuaPerfStats), plus the layout, batch build, draw and texture upload phases of the UI frame (GetUiPerfStats).", false,
		    [](Cvar& c)
		{
			sLuaRuntime.SetStatsEnabled(c.AsBool());
		});

		sCvars.Register("luaPerfHitchMs",
		    "With luaPerfStats on, write any frame whose Lua handler plus GC time, or whose UI update plus build plus draw time, reaches this many ms as a row to Logs/uiperf_<client start time>.log, a new file per client start. 0 disables.", "0",
		    [](Cvar& c)
		{
			sLuaRuntime.SetHitchMs(c.AsFloat());
		});

		sCvars.Register("luaProfile",
		    "1 times every Lua and C function call through a Lua call/return hook and writes the top functions by self time under each hitch row of the uiperf log (needs luaPerfStats and luaPerfHitchMs). Costs a few percent of Lua time while on. 0 off.", 0,
		    [](Cvar& c)
		{
			sLuaRuntime.SetProfile(c.AsInt());
		});
	}
}
