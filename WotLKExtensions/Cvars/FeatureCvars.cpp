#include "FeatureCvars.h"

#include "LootWindowCvar.h"
#include "DiscordCvar.h"
#include "StartupDownloaderCvar.h"
#include "DebugOutputCvar.h"
#include "ActionRepeatCvar.h"

namespace FeatureCvars
{
	void Apply()
	{
		LootWindowCvar::Apply();
		DiscordCvar::Apply();
		StartupDownloaderCvar::Apply();
		DebugOutputCvar::Apply();
		ActionRepeatCvar::Apply();
	}
}
