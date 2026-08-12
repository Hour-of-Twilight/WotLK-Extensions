#include "AutoRepeatDeadzone.h"

#include <ClientData/AutoRepeat.h>
#include <Helpers/Util.h>

using namespace ClientData;

namespace
{
	int __fastcall MeleeRangeHook(CGUnit* self, void*, CGUnit* target)
	{
		int percent = sAutoRepeatDeadzone.Percent();

		if (percent >= 100)
			return AutoRepeat::IsInMeleeRange(self, target);

		if (percent <= 0 || !self || !target)
			return 0;

		float radius = AutoRepeat::StockMeleeRadius(self, target) * (percent / 100.0f);
		return AutoRepeat::DistanceSquared(self, target) <= radius * radius ? 1 : 0;
	}
}

AutoRepeatDeadzone& AutoRepeatDeadzone::Instance()
{
	static AutoRepeatDeadzone instance;
	return instance;
}

void AutoRepeatDeadzone::SetPercent(int percent)
{
	m_percent = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
}

void AutoRepeatDeadzone::Apply()
{
	Util::OverwriteUInt32AtAddress(AutoRepeat::MELEE_RANGE_CALL_REL32,
	    reinterpret_cast<uint32_t>(&MeleeRangeHook) - AutoRepeat::MELEE_RANGE_CALL_NEXT);

	// Movement is left to CheckToCancelSpellsDueToMovement, which honours InterruptFlags.
	Util::SetByteAtAddress(reinterpret_cast<void*>(AutoRepeat::MOVEMENT_GATE_MASK), 0x00);
}
