#include "PlayerRenderLimitCvar.h"

#include <Cvars.h>
#include <Character/PlayerRenderLimit.h>

namespace PlayerRenderLimitCvar
{
	void Apply()
	{
		sCvars.Register("playerRenderLimit",
		    "Most other players drawn at once. Party, raid, friends and guild members are kept first, then the nearest. Clamped to 20 through 100.", 100,
		    [](Cvar& c)
		{
			sPlayerRenderLimit.SetLimit(c.AsInt());
		});
	}
}
