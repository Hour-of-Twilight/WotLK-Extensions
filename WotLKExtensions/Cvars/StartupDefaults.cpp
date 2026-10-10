#include "StartupDefaults.h"

#include <ClientData/ClientFunctions.h> // CVar_C::GetCVar, CVar_C::SetCvar
#include <ClientData/System.h>          // ClientData::CVar
#include <Config/LauncherSettings.h>
#include <filesystem>

using namespace ClientData;

namespace
{
	void SetValue(const char* name, const char* value, bool updateConfig = true)
	{
		CVar* cvar = CVar_C::GetCVar(name);
		if (!cvar)
			return;

		CVar_C::SetCvar(cvar, value, /*setValue*/ true, /*setReset*/ false, /*setDefault*/ true, updateConfig);
	}

	// True only when this launch found no config of its own. The client reads the config
	// before this runs and does not write one until it exits, so a missing Config.wtf here
	// means a new install. Anything less certain than that - including not knowing where we
	// are installed - counts as "not a first run".
	bool IsFreshInstall()
	{
		std::error_code ec;

		std::filesystem::path root = sLauncherSettings.InstallDir();
		if (root.empty())
			root = std::filesystem::current_path(ec);
		if (root.empty())
			return false;

		return !std::filesystem::exists(root / "WTF" / "Config.wtf", ec);
	}
}

namespace StartupDefaults
{
	// Runs from Main::OnAttach, which is hooked onto ConsoleDeviceInitialize before the
	// original runs: the config has been read, so these CVars exist and a write sticks.
	//
	// Display mode does not belong here. gxWindow and gxMaximize do not exist yet at this
	// point, and setting them just after ConsoleDeviceInitialize returns does not take
	// either: the write bumps the CVar's modified counter and leaves the value at 0, the
	// client having already built the device. The glue UI sets the display mode instead,
	// through the same path the video options use (GlueXML/HotFirstRun.lua).
	void Apply()
	{
		// No startup cinematic. The glue UI cannot do this on a fresh config, because the
		// client reads these before GlueXML loads, and escaping the movie screen after it
		// opens leaves the MSDF text resources released for the rest of the session.
		SetValue("movie", "0");
		SetValue("expansionMovie", "0");

		// What the glue UI's first-run flow keys off. Deliberately not written to the
		// config: it is a fact about this launch, recomputed on every one of them.
		SetValue("firstRunFreshInstall", IsFreshInstall() ? "1" : "0", /*updateConfig*/ false);
	}
}
