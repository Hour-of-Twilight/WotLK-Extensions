#include "Spell.h"
#include "SharedDefines.h"
#include <string>
#include "Player.h"
#include <ClientDetours.h>

static bool __cdecl SpellIgnoresMovementGate(SpellRow* spell)
{
	return spell && sPlayer.CanCastWhileMoving(spell);
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

void Spells::Apply()
{

    const uintptr_t gateEntry = 0x0073A042;
	uint8_t patch[9];
	int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&CGUnit_C__MovementGate) - (gateEntry + 5));
	patch[0] = 0xE9; // jmp rel32
	std::memcpy(&patch[1], &rel, sizeof(rel));
	std::memset(&patch[5], 0x90, 4);
	Util::OverwriteBytesAtAddress(static_cast<uint32_t>(gateEntry), patch, sizeof(patch));

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
