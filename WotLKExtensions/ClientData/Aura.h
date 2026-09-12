#pragma once

#include <Macros.h>
#include <ClientData/Object.h>
#include <ClientData/Spell.h>

#include <cstdint>

namespace ClientData
{
	namespace Aura
	{
		constexpr uint32_t MaxSpellEffects = 3;
		constexpr uint32_t MaxAuraSlots = 64;

		enum AuraType : uint32_t
		{
			SPELL_AURA_SCHOOL_ABSORB = 69,
			SPELL_AURA_MANA_SHIELD = 97,
			SPELL_AURA_SCHOOL_HEAL_ABSORB = 301,
		};

		// SetTooltipUnitAura's "call CGTooltip__SetBuff", where esi is the CGUnit and edi the slot.
		constexpr uint32_t SetBuffCallSite = 0x00625FF7;

		// The dword CSimpleFrame_IsVisible tests.
		constexpr uint32_t FrameVisibleOffset = 0xE0;

		struct TooltipCall
		{
			void* tooltip;
			uint64_t unitGuid;
			int32_t slot;
			uint32_t spellId;
			bool valid;
		};

		inline bool IsFrameVisible(void* frame)
		{
			if (!frame)
				return false;

			return *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(frame) + FrameVisibleOffset) != 0;
		}
	}
}
