#include "Spell.h"
#include "SharedDefines.h"
#include <string>
#include "Player.h"
#include <ClientDetours.h>

static bool __cdecl SpellIgnoresMovementGate(SpellRow* spell)
{
	return spell && sPlayer.CanCastWhileMoving(spell);
}

// strip the equipped item requirements the caster carries an ignore mod for. a class of -1 or an
// empty subclass mask both read as "no requirement at all" everywhere the client checks them
bool Spells::RelaxEquippedItemRequirements(SpellRow* spell)
{
	if (!spell)
		return false;

	int32_t itemClass = (int32_t)spell->m_equippedItemClass;
	bool hasRequirement = itemClass >= 0 && itemClass < 32 && spell->m_equippedItemSubclass != 0;

	// these pin the check to a single equipment slot rather than scanning all of them, and the
	// client enforces them on their own, so waiving the weapon class has to drop them too
	uint32_t attr0Slot = spell->m_attributes & SPELL_ATTR0_REQ_AMMO;
	uint32_t attr3Slot = spell->m_attributesExC & (SPELL_ATTR3_MAIN_HAND | SPELL_ATTR3_REQ_OFFHAND);

	if (!hasRequirement && !attr0Slot && !attr3Slot)
		return false;

	uint32_t classMod = sPlayer.GetSpellModMask(spell, SPELLMOD_IGNORE_ITEM_CLASS);
	if (!(classMod & (1u << ITEM_CLASS_WEAPON)))
	{
		attr0Slot = 0;
		attr3Slot = 0;
	}

	uint32_t ignoreClass = 0;
	uint32_t ignoreSubclass = 0;
	uint32_t ignoreInvType = 0;
	if (hasRequirement)
	{
		ignoreClass = classMod & (1u << itemClass);
		ignoreSubclass = sPlayer.GetSpellModMask(spell, SPELLMOD_IGNORE_ITEM_SUBCLASS) & spell->m_equippedItemSubclass;
		ignoreInvType = sPlayer.GetSpellModMask(spell, SPELLMOD_IGNORE_ITEM_INV_TYPE) & spell->m_equippedItemInvTypes;
	}

	if (!ignoreClass && !ignoreSubclass && !ignoreInvType && !attr0Slot && !attr3Slot)
		return false;

	if (ignoreClass)
		spell->m_equippedItemClass = (uint32_t)-1;
	spell->m_equippedItemSubclass &= ~ignoreSubclass;
	spell->m_equippedItemInvTypes &= ~ignoreInvType;
	spell->m_attributes &= ~attr0Slot;
	spell->m_attributesExC &= ~attr3Slot;
	return true;
}

// the cost value stays as the row authored it, only the pot it comes out of changes. everything
// downstream of the row - the tooltip cost line and its token, the cost calls, the usability
// check, the out of power error - reads the power type back off the row, so this is all it takes
bool Spells::ApplyPowerTypeMod(SpellRow* spell)
{
	if (!spell)
		return false;

	int32_t powerType = 0;
	if (!sPlayer.GetSpellPowerType(spell, powerType) || (uint32_t)powerType == spell->m_powerType)
		return false;

	spell->m_powerType = (uint32_t)powerType;
	return true;
}

static void __cdecl RelaxStackSpellRow(SpellRow* spell)
{
	Spells::RelaxEquippedItemRequirements(spell);
}

// the cast handlers run for everyone's spells, and the mods we hold only describe our own
static void __cdecl PatchStackSpellRowPowerType(SpellRow* spell, uint32_t guidLow, uint32_t guidHigh)
{
	uint64_t caster = ((uint64_t)guidHigh << 32) | guidLow;
	if (caster != ClntObjMgr::GetActivePlayer())
		return;

	Spells::ApplyPowerTypeMod(spell);
}

// CGTooltip__SetSpell holds its SpellRow on the stack at ebp-0x488 and runs the same three guards
// as the cast check right here, so relaxing the row in place decides the "Requires x" line as well.
// the overwritten instruction is the first of those guards and has to be replayed on the way out
__declspec(naked) void CGTooltip__SetSpell_EquippedItemGate()
{
	__asm {
        pushad
        pushfd
        lea  eax, [ebp-0x488]
        push eax
        call RelaxStackSpellRow
        add  esp, 4
        popfd
        popad

        test byte ptr [ebp-0x448], 0x10
        push 0x006248AA
        ret
	}
}

// Spell_C_CastSpell copies the row onto its own stack at ebp-0x2B8 and then enforces some of the
// requirements itself instead of leaving it all to Spell_C_HaveEquippedSpellItems, the ranged
// weapon check behind SPELL_ATTR0_REQ_AMMO among them. relaxing the copy right after it is loaded
// covers every one of those. the overwritten instruction is the first read of the row
__declspec(naked) void Spell_C_CastSpell_RelaxSpellRow()
{
	__asm {
        pushad
        pushfd
        lea  eax, [ebp-0x2B8]
        push eax
        call RelaxStackSpellRow
        add  esp, 4
        popfd
        popad

        test byte ptr [ebp-0x2A8], 0x40
        push 0x0080CD18
        ret
	}
}

// the two spell cast handlers below build their own copy of the row instead of going through
// ClientDb::GetLocalizedRow, and then drop the server's cost onto whichever power slot the copy
// names. both hooks sit where the compressed and the plain copy paths meet, so the row is filled
// in either way, and both replay the two instructions they overwrite

// SMSG_SPELL_GO, row at ebp-0x2E4, caster guid in the two slots the replayed instructions load
__declspec(naked) void Spell_C_HandleSpellGo_PowerType()
{
	__asm {
        pushad
        pushfd
        push dword ptr [ebp-0x08]
        push dword ptr [ebp-0x0C]
        lea  eax, [ebp-0x2E4]
        push eax
        call PatchStackSpellRowPowerType
        add  esp, 12
        popfd
        popad

        mov  ecx, [ebp-0x08]
        mov  edx, [ebp-0x0C]
        push 0x00806877
        ret
	}
}

// SMSG_SPELL_START, row at ebp-0x3B0
__declspec(naked) void Spell_C_HandleSpellStart_PowerType()
{
	__asm {
        pushad
        pushfd
        push dword ptr [ebp-0x34]
        push dword ptr [ebp-0x38]
        lea  eax, [ebp-0x3B0]
        push eax
        call PatchStackSpellRowPowerType
        add  esp, 12
        popfd
        popad

        mov  ecx, [ebp-0x34]
        mov  edx, [ebp-0x38]
        push 0x0080E62A
        ret
	}
}

__declspec(naked) void CGUnit_C__MovementGate()
{
	__asm {
        pushad
        push esi
        call SpellIgnoresMovementGate
        add  esp, 4
        test al, al
        jnz  bypass

        popad
        mov  dl, 0x60
        mov  ecx, 0x00C0100F
        test eax, eax
        push 0x0073A04B
        ret

    bypass:
        popad
        push 0x0073A0F0
        ret
	}
}

// jmp rel32 to hook, nops out to len so nothing lands mid instruction
static void WriteJumpPatch(uintptr_t at, void* hook, size_t len)
{
	uint8_t patch[16];
	int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(hook) - (at + 5));
	patch[0] = 0xE9;
	std::memcpy(&patch[1], &rel, sizeof(rel));
	std::memset(&patch[5], 0x90, len - 5);
	Util::OverwriteBytesAtAddress(static_cast<uint32_t>(at), patch, len);
}

void Spells::Apply()
{
	WriteJumpPatch(0x0073A042, &CGUnit_C__MovementGate, 9);
	WriteJumpPatch(0x006248A3, &CGTooltip__SetSpell_EquippedItemGate, 7);
	WriteJumpPatch(0x0080CD11, &Spell_C_CastSpell_RelaxSpellRow, 7);
	WriteJumpPatch(0x00806871, &Spell_C_HandleSpellGo_PowerType, 6);
	WriteJumpPatch(0x0080E624, &Spell_C_HandleSpellStart_PowerType, 6);

	// g_spell_min_clip_distance_percentage_cvar = CVar_C::Register("spellMinClipDistancePercentage", "Sets the minimum distance the clipping needs to be to activate", 1, "0.0", SpellMinClipDistancePercentage_CVarCallback, 5, 0, 0, 0);
}

char Spells::SpellMinClipDistancePercentage_CVarCallback(CVar* cvar, const char*, const char* value, const char*)
{
	const float clip = std::atof(value);
	if (clip != std::clamp(clip, 0.0f, 1.0f))
		return 0;
	cvar->m_numberValue = clip;
	return 1;
}
