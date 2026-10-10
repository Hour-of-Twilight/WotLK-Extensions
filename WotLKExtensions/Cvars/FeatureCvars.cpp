#include "FeatureCvars.h"

#include "LootWindowCvar.h"
#include "DiscordCvar.h"
#include "StartupDownloaderCvar.h"
#include "DebugOutputCvar.h"
#include "ActionRepeatCvar.h"
#include "MeleeDeadzoneCvar.h"
#include "HealPredictionCvars.h"
#include "ComboPointCvar.h"
#include "FirstRunCvars.h"
#include "StartupDefaults.h"
#include "PlayerRenderLimitCvar.h"

namespace FeatureCvars
{
	void Apply()
	{
		LootWindowCvar::Apply();
		DiscordCvar::Apply();
		StartupDownloaderCvar::Apply();
		DebugOutputCvar::Apply();
		ActionRepeatCvar::Apply();
		MeleeDeadzoneCvar::Apply();
		HealPredictionCvars::Apply();
		ComboPointCvar::Apply();
		FirstRunCvars::Apply();
		StartupDefaults::Apply();
		PlayerRenderLimitCvar::Apply();
	}
}
