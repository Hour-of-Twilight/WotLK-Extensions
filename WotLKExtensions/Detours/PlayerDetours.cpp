#include <ClientDetours.h>
#include <SharedDefines.h>
#include <Item.h>
#include <Player.h>
#include "Logger.h"

CLIENT_DETOUR(GetComboPoints, 0x00513920, __cdecl, uint8_t, (uint64_t /*targetGuid*/))
{
	return *g_comboPointCount;
}

CLIENT_DETOUR(Script_GetSpellBonusHealing, 0x0060E3B0, __cdecl, int, (lua_State * L))
{
	if (FrameScript::GetTop(L) < 1)
		return Script_GetSpellBonusHealing(L);

	int32_t school = static_cast<int32_t>(FrameScript::GetNumber(L, 1)) - 1;
	if (school < 0 || school >= static_cast<int32_t>(MAX_SPELL_SCHOOL))
	{
		FrameScript::DisplayError(L, "Usage: GetSpellBonusHealing(school)");
		return 0;
	}

	FrameScript::PushNumber(L, sPlayer.GetCustomSpellHealing(static_cast<uint8>(school)));
	return 1;
}
