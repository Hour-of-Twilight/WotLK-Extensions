#include <Character/HDCharacters/HDPatchToggle.h>

#include <Character/HDCharacters/HDCharacters.h>
#include <Config/LauncherSettings.h>
#include <Logger.h>

namespace HDPatchToggle
{
	void Apply()
	{
		HDCharacters::SetEnabled(sLauncherSettings.HdPatch());
	}

	void Refresh()
	{
		bool wanted = sLauncherSettings.HdPatch();
		bool available = HDCharacters::RefreshAvailability();
		if (wanted && !available)
			LOG_INFO << "hd characters: the hd patch is on but its art is not here yet, the models switch over as soon as it lands";
		HDCharacters::SetEnabled(wanted);
	}
}
