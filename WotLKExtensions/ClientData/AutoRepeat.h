#pragma once

#include <Macros.h>
#include <ClientData/MathTypes.h>
#include <ClientData/ObjectFields.h>
#include <ClientData/Units.h>
#include <ClientData/VectorMath.h>

#include <cstdint>

namespace ClientData::AutoRepeat
{
	CLIENT_ADDRESS(uint32_t, g_autoRepeatSpell, 0x00D397CC)

	// 0 for melee-only classes, which makes the combat mode handler early-return.
	CLIENT_ADDRESS(uint32_t, g_autoRangedCombatSpell, 0x00BE5D84)

	CLIENT_FUNCTION(GetAutoRepeatingSpell, 0x007FE130, __cdecl, uint32_t, ())
	CLIENT_FUNCTION(SetAutoRepeatingSpell, 0x00800A00, __cdecl, void, (uint32_t spellId))
	CLIENT_FUNCTION(CancelAutoRepeat, 0x00807560, __cdecl, void, (int sendToServer))
	CLIENT_FUNCTION(CancelRangedSpells, 0x00806550, __cdecl, void, ())
	CLIENT_FUNCTION(CancelMeleeSpells, 0x00806480, __cdecl, void, ())

	CLIENT_FUNCTION(IsInMeleeRange, 0x0071B820, __thiscall, int, (CGUnit * self, CGUnit* target))

	// Ends in CancelAutoRepeat(1), which is what kills auto shot on melee-range entry.
	CLIENT_FUNCTION(MeleeModeEnter, 0x006E2610, __thiscall, void, (CGUnit * self, uint64_t* targetGuid))

	// flt_9EBF34 and flt_9F987C.
	constexpr float MELEE_RANGE_FLOOR = 5.0f;
	constexpr float MELEE_RANGE_BONUS = 1.3333334f;

	// CGObject_C vtable slot, as a byte offset, that writes the world position to an out param.
	constexpr uintptr_t VF_GET_POSITION = 0x2C;

	using GetPositionFn = C3Vector*(__thiscall*)(CGUnit * self, C3Vector* out);

	// In CGPlayer_C::AutoCombatModeEventHandler (0x006E2BE0), the IsInMeleeRange call whose
	// true branch runs MeleeModeEnter. The other call site (0x006E4B51) is left stock.
	constexpr uintptr_t MELEE_RANGE_CALL_REL32 = 0x006E2C65;
	constexpr uintptr_t MELEE_RANGE_CALL_NEXT = 0x006E2C69;

	// Mask byte of `test byte ptr [eax+44h], 33h`, the movement gate in front of the
	// Spell_C_CastSpell that starts auto-repeat.
	constexpr uintptr_t MOVEMENT_GATE_MASK = 0x006E2DC0;

	inline C3Vector GetPosition(CGUnit* unit)
	{
		C3Vector out{};
		uint8_t* vtable = *reinterpret_cast<uint8_t**>(unit);
		auto getPosition = *reinterpret_cast<GetPositionFn*>(vtable + VF_GET_POSITION);
		getPosition(unit, &out);
		return out;
	}

	inline float DistanceSquared(CGUnit* self, CGUnit* target)
	{
		C3Vector delta = VectorMath::Subtract(GetPosition(target), GetPosition(self));
		return VectorMath::Dot(delta, delta);
	}

	// max(5.0, self.combatReach + target.combatReach + 1.3333334), what IsInMeleeRange
	// compares the squared distance against.
	inline float StockMeleeRadius(CGUnit* self, CGUnit* target)
	{
		if (!self || !target || !self->unitData || !target->unitData)
			return MELEE_RANGE_FLOOR;

		float reach = self->unitData->combatReach + target->unitData->combatReach + MELEE_RANGE_BONUS;
		return reach > MELEE_RANGE_FLOOR ? reach : MELEE_RANGE_FLOOR;
	}
}
