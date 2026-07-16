#pragma once

#include <cstdint>

int MultiCastBar_GetTotemSlotMask(uint32_t spellId);

inline bool MultiCastBar_IsTotemSpell(uint32_t spellId)
{
	return MultiCastBar_GetTotemSlotMask(spellId) != 0;
}
