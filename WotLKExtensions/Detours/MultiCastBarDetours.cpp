#include "ClientDetours.h"
#include "MultiCastBarDetours.h"

#include <SharedDefines.h>
#include <Spell.h>
#include <Player.h>
#include <Util.h>

#include <cstdint>
#include <cstring>


static constexpr uint32_t kSummonPropertiesDB = 0x00AD4B38 + 0x18;

// Match server SharedDefines.
static constexpr uint32_t SPELL_EFFECT_SUMMON = 28;
static constexpr int32_t SUMMON_TYPE_TOTEM = 4;
static constexpr int32_t SUMMON_SLOT_TOTEM_FIRE = 1; // fire=1, earth=2, water=3, air=4

struct SummonPropertiesRow
{
	int32_t m_ID;
	int32_t m_control;
	int32_t m_faction;
	int32_t m_title;
	int32_t m_slot;
	int32_t m_flags;
};

static int s_totemSlotMask = 0;
static bool s_totemMaskConsumed = false;

static bool ReadSpellRow(uint32_t spellId, SpellRow* out)
{
	if (!g_SpellDB || !g_SpellDB->Rows)
		return false;
	if (spellId < (uint32_t)g_SpellDB->minIndex || spellId > (uint32_t)g_SpellDB->maxIndex)
		return false;

	void* rec = ((void**)g_SpellDB->Rows)[spellId - g_SpellDB->minIndex];
	if (!rec)
		return false;

	if (*g_isDBCCompressed)
		ClientDB::DecompressRow(rec, 0x2A8, out);
	else
		std::memcpy(out, rec, 0x2A8);
	return true;
}

// Returns the multicast element bit (fire=0x1, earth=0x2, water=0x4, air=0x8) for
// a totem spell, or 0 if the spell doesn't summon a totem.
int MultiCastBar_GetTotemSlotMask(uint32_t spellId)
{
	SpellRow spell;
	if (!ReadSpellRow(spellId, &spell))
	{
		Util::DebugOutput("[MCBar] DeriveTotemSlotMask spell %u: could not read spell row", spellId);
		return 0;
	}

	Util::DebugOutput("[MCBar] DeriveTotemSlotMask spell %u: attrExG=0x%08X effects=[%u,%u,%u]",
	    spellId, spell.m_attributesExG, spell.m_effect[0], spell.m_effect[1], spell.m_effect[2]);

	for (int i = 0; i < 3; ++i)
	{
		if (spell.m_effect[i] != SPELL_EFFECT_SUMMON)
			continue;

		uint32_t summonPropId = spell.m_effectMiscValueB[i];
		if (!summonPropId)
			continue;

		auto* props = (SummonPropertiesRow*)ClientDB::GetRow((void*)kSummonPropertiesDB, summonPropId);
		if (!props)
		{
			Util::DebugOutput("[MCBar]   effect %d summon: summonPropId=%u NOT FOUND in client dbc", i, summonPropId);
			continue;
		}

		Util::DebugOutput("[MCBar]   effect %d summon: summonPropId=%u title=%d slot=%d",
		    i, summonPropId, props->m_title, props->m_slot);

		if (props->m_title != SUMMON_TYPE_TOTEM)
			continue;

		int slot = props->m_slot - SUMMON_SLOT_TOTEM_FIRE; // fire=0..air=3
		if (slot < 0 || slot > 3)
			continue;

		Util::DebugOutput("[MCBar]   -> totem slot %d, mask 0x%X", slot, 1 << slot);
		return 1 << slot;
	}
	return 0;
}

CLIENT_DETOUR(CGSpellBook_AddKnownSpell, 0x00542030, __cdecl, void, (int spellId, int a2, int a3))
{
	int prevMask = s_totemSlotMask;
	bool prevConsumed = s_totemMaskConsumed;

	s_totemSlotMask = MultiCastBar_GetTotemSlotMask((uint32_t)spellId);
	s_totemMaskConsumed = false;

	Util::DebugOutput("[MCBar] AddKnownSpell spell=%d derivedMask=0x%X", spellId, s_totemSlotMask);

	CGSpellBook_AddKnownSpell(spellId, a2, a3);

	s_totemSlotMask = prevMask;
	s_totemMaskConsumed = prevConsumed;
}

CLIENT_DETOUR(GetMultiCastSlotMaskForTotemCategory, 0x005A7B50, __cdecl, int, (int totemCategory))
{
	int mask = GetMultiCastSlotMaskForTotemCategory(totemCategory);

	Util::DebugOutput("[MCBar] GetMultiCastSlotMask cat=%d nativeMask=0x%X stashed=0x%X consumed=%d",
	    totemCategory, mask, s_totemSlotMask, (int)s_totemMaskConsumed);

	if (mask != 0)
		return mask;

	if (s_totemSlotMask != 0 && !s_totemMaskConsumed)
	{
		s_totemMaskConsumed = true;
		Util::DebugOutput("[MCBar] GetMultiCastSlotMask injecting mask 0x%X", s_totemSlotMask);
		return s_totemSlotMask;
	}
	return 0;
}

CLIENT_DETOUR(MultiCastValidateTotemForSlot, 0x005A7BC0, __cdecl, int, (int slot, SpellRow* spellRec, uint32_t* outCategory))
{
	int result = MultiCastValidateTotemForSlot(slot, spellRec, outCategory);
	if (result)
		return result;

	if (spellRec && slot >= 0x84 && slot < 0x90)
	{
		int mask = MultiCastBar_GetTotemSlotMask(spellRec->m_ID);
		if (mask != 0 && mask == (1 << ((slot - 0x84) % 4)))
		{
			Util::DebugOutput("[MCBar] ValidateTotemForSlot spell %u accepted for slot 0x%X (mask 0x%X)",
			    spellRec->m_ID, slot, mask);
			return 1;
		}
	}
	return result;
}
