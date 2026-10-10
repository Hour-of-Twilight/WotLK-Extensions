#pragma once

#include <Defines.h>
#include "SharedDefines.h"
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <algorithm>

class Spells
{
public:
	static inline bool s_castAtCursor = false;
	static inline CVar* g_spell_min_clip_distance_percentage_cvar;
	static char SpellMinClipDistancePercentage_CVarCallback(CVar* cvar, const char*, const char* value, const char*);
	// mutates spell in place, true if any equipped item requirement was waived
	static bool RelaxEquippedItemRequirements(SpellRow* spell);
	// mutates spell in place, true if the caster's mods moved it onto another power type
	static bool ApplyPowerTypeMod(SpellRow* spell);
	static uint32 GetCustomAttributes(uint32 spellId);
	static uint32 GetCustomAttributes2(uint32 spellId);
	static bool IsDualWielding();
	static bool MeetsCustomAttributeRequirements(SpellRow* spell, int32 reportError, void* spellCast);
	static void AddDualWieldTooltipLine(void* tooltip, SpellRow* spell);
	static void Apply();
};
