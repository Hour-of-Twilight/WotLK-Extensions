#include "ComboPointCvar.h"

#include <Cvars.h>
#include <XMLExtensions.h>

namespace ComboPointCvar
{
	static const char* const kAnchorEvent = "HOT_COMBO_POINT_ANCHOR";

	static void OnAnchorChanged(Cvar& cvar)
	{
		if (FrameXMLExtensions::GetEventIdByName(kAnchorEvent) < 0)
			return;

		FrameXMLExtensions::SignalEvent(kAnchorEvent, "%s", cvar.AsString());
	}

	void Apply()
	{
		sCvars.Register("comboPointAnchor", "Unit frame the combo point display sits on: player or target.", "player",
		    &OnAnchorChanged);
	}
}
