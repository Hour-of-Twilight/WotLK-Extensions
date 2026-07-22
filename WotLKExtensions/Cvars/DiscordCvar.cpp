#include "DiscordCvar.h"

#include <Cvars.h>

#ifdef ENABLE_DISCORD
#include <Util.h>
#include <DiscordRPC.h>
#endif

namespace DiscordCvar
{
#ifdef ENABLE_DISCORD
	static void Sync(bool enabled)
	{
		// Discord never runs under Wine even when the cvar is on.
		if (enabled && !Util::IsWine())
		{
			if (!sDiscord.IsInitialized())
			{
				sDiscord.Init();
				sDiscord.UpdateActivity("", "Staring at the login screen.", "icon_transparent_");
			}
		}
		else if (sDiscord.IsInitialized())
		{
			sDiscord.Shutdown();
		}
	}
#endif

	void Apply()
	{
#ifdef ENABLE_DISCORD
		sCvars.Register("enableDiscord", "Enable Discord Rich Presence.", true,
		    [](Cvar& c)
		{
			Sync(c.AsBool());
		});
#endif
	}

	void SetEnabled(bool enabled)
	{
#ifdef ENABLE_DISCORD
		if (Cvar* c = sCvars.Find("enableDiscord"))
			c->Set(enabled);
#else
		(void)enabled;
#endif
	}
}
