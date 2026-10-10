#pragma once

#include <Defines.h>

#include <cstdint>

int MultiCastBar_GetTotemSlotMask(uint32 spellId);

inline bool MultiCastBar_IsTotemSpell(uint32 spellId)
{
	return MultiCastBar_GetTotemSlotMask(spellId) != 0;
}
