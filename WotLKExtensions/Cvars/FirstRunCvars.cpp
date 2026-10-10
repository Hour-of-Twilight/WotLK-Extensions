#include "FirstRunCvars.h"

#include <Cvars.h>

namespace FirstRunCvars
{
	// Storage for the glue screens' one-off first-run prompts (GlueXML/HotFirstRun.lua).
	// Nothing in the DLL reacts to these; they exist so the prompts survive a restart.
	void Apply()
	{
		sCvars.Register("firstRunVideoShown", "Set once the first-run video setup has been shown.", false);
		sCvars.Register(
		    "firstRunHdPatchOffered", "Set once the HD patch has been offered on the first connection.", false);

		// Recomputed every launch by StartupDefaults from whether WTF/Config.wtf existed
		// at startup, and never written to the config. The "already shown" flags above
		// cannot carry the first-run decision on their own: a launch that never writes
		// its config - a crash, or the player killing the process - loses them, and the
		// video setup would open again on a long-installed client.
		sCvars.Register("firstRunFreshInstall", "Set when this launch found no config, i.e. a new install.", false);
	}
}
