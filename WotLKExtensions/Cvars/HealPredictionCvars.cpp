#include "HealPredictionCvars.h"

#include <Cvars.h>

namespace HealPredictionCvars
{
	void Apply()
	{
		sCvars.Register("healPredictionAbsorb", "Draw absorb shields on unit frame health bars.", true);
		sCvars.Register("healPredictionAbsorbFullHealth",
		    "Keep drawing the absorb shield when the unit is at full health, capped at the width of the health bar.", false);
		sCvars.Register("healPredictionAbsorbText", "Show the total absorb amount as text on the health bar.", false);
		sCvars.Register("healPredictionHealText", "Show the predicted incoming heal amount as text on the health bar.", false);
		sCvars.Register("healPredictionAbsorbColor", "Tint for the absorb shield marker, as rrggbb hex.", "ffffff");
		sCvars.Register("healPredictionHealAbsorbColor", "Tint for the heal absorb marker, as rrggbb hex.", "ffffff");
		sCvars.Register("healPredictionMyHealColor", "Colour of your own incoming heal marker, as rrggbb hex.", "00a89b");
		sCvars.Register("healPredictionOtherHealColor", "Colour of other players' incoming heal markers, as rrggbb hex.", "155947");
		sCvars.Register("healPredictionAbsorbTextColor", "Colour of the absorb amount text, as rrggbb hex.", "66b3ff");
		sCvars.Register("healPredictionHealTextColor", "Colour of the incoming heal amount text, as rrggbb hex.", "ffd100");
	}
}
