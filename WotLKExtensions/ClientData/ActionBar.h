#pragma once

#include <Macros.h>

#include <cstdint>

namespace ClientData::ActionBar
{
	constexpr uintptr_t kUseActionAddr = 0x005ABBC0;

	CLIENT_FUNCTION(GetCooldown, 0x005A8E40, __cdecl, void, (int slot, int* outStart, int* outDuration, int* outEnable))

	CLIENT_ADDRESS(int, s_currentPage, 0x00C1E598)    // 0-based page
	CLIENT_ADDRESS(int, s_bonusBarOffset, 0x00C1E59C) // 0 when no stance/form bar
	CLIENT_ADDRESS(int, s_tempPageActiveFlags, 0x00C1E5A0)

	constexpr int kNumButtons = 12;
	constexpr int kNumPages = 6;
}
