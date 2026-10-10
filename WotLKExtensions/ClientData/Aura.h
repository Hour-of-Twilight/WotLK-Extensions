#pragma once

#include <Defines.h>

#include <Macros.h>
#include <ClientData/Object.h>
#include <ClientData/Spell.h>

#include <cstdint>

namespace ClientData
{
	namespace Aura
	{
		constexpr uint32 MaxSpellEffects = 3;
		constexpr uint32 MaxAuraSlots = 64;

		enum AuraType : uint32
		{
			SPELL_AURA_SCHOOL_ABSORB = 69,
			SPELL_AURA_MANA_SHIELD = 97,
			SPELL_AURA_SCHOOL_HEAL_ABSORB = 301,
			SPELL_AURA_TEMP_LEARN_SPELL = 317,
			SPELL_AURA_MONK_ANIMS = 336,
		};

		// SetTooltipUnitAura's "call CGTooltip__SetBuff", where esi is the CGUnit and edi the slot.
		constexpr uint32 SetBuffCallSite = 0x00625FF7;

		// The dword CSimpleFrame_IsVisible tests.
		constexpr uint32 FrameVisibleOffset = 0xE0;

		struct TooltipCall
		{
			void* tooltip;
			uint64 unitGuid;
			int32 slot;
			uint32 spellId;
			bool valid;
		};

		inline bool IsFrameVisible(void* frame)
		{
			if (!frame)
				return false;

			return *reinterpret_cast<uint32*>(reinterpret_cast<uint8*>(frame) + FrameVisibleOffset) != 0;
		}
	}
}
