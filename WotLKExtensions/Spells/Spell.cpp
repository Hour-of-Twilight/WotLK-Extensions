#include "Spell.h"
#include "SharedDefines.h"
#include <string>
#include "Player.h"
#include <ClientDetours.h>

bool __cdecl CheckMovementInterrupt(void* self, SpellRow* pSpellRec, void* pUnitData)
{
	void* fuck = *reinterpret_cast<void**>((char*)self + 0xD8);
	uint32_t shit = *reinterpret_cast<uint32_t*>((char*)fuck + 0x44);

	if ((shit & *unk_C0100F) &&
	    !(pSpellRec->m_attributesExC & 0x60) &&
	    !(pSpellRec->m_attributesExB & 0x20))
	{

		uint32_t hasInstantCast = (*reinterpret_cast<uint32_t*>((char*)pUnitData + 0x114) != 0);
		int castTime = Spell_C::GetCastTime(pSpellRec, hasInstantCast, 0, 0);

		if (castTime > 0 || (pSpellRec->m_auraInterruptFlags & 0x1800))
		{
			return false;
		}
	}

	return true;
}

__declspec(naked) void MovementCheckHook()
{
	__asm {
        pushad

        mov eax, [ebp - 0x04]
        mov ecx, [eax + 0xD0]
        push ecx
        push esi
        push eax

        call CheckMovementInterrupt
        add esp, 12

        mov byte ptr[esp + 28], al
        popad

        test al, al
        jnz allow_movement

        push 0
        push 0FFFFFFFFh
        push 0FFFFFFFFh
        push 33h
        push esi
        mov eax, [ebp + 8]
        push eax
        call Spell_C::SpellFailed
        add esp, 18h

        push 0x7397C5
        ret

        allow_movement :
        push 0x73A0F0
            ret
	}
}

void Spells::Apply()
{

	uint32_t hookAddr = 0x73A086;
	uint32_t funcAddr = reinterpret_cast<uint32_t>(&MovementCheckHook);

	uint8_t jmpPatch[5];
	jmpPatch[0] = 0xE9;
	int32_t offset = funcAddr - (hookAddr + 5);
	memcpy(&jmpPatch[1], &offset, 4);

	Util::OverwriteBytesAtAddress(hookAddr, jmpPatch, 5);

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
