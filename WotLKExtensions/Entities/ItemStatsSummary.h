#pragma once

#include <SharedDefines.h>
#include <Misc.h>

// Side storage for custom item mods (types 49-72). CGItemStatsSummary is a fixed 0x12C
// bytes with no room for them, so they hang off the summary pointer instead.
namespace ItemStatsSummary
{
	static constexpr int CUSTOM_STAT_FIRST = 49; // ITEM_MOD_MAGIC_FIND
	static constexpr int CUSTOM_STAT_COUNT = static_cast<int>(ADDITIONAL_STATS);
	static constexpr int CUSTOM_STAT_LAST = CUSTOM_STAT_FIRST + CUSTOM_STAT_COUNT - 1;
	static constexpr int VANILLA_STAT_LAST = 48; // ITEM_MOD_BLOCK_VALUE
	static constexpr int SUMMARY_SIZE = 0x12C;
	static constexpr int SUMMARY_ARRAY_COUNT = 73;

	static constexpr int ITEM_MOD_ATTACK_POWER = 38;
	static constexpr int ITEM_MOD_RANGED_ATTACK_POWER = 39;
	static constexpr int ITEM_CACHE_STAT_COUNT = 10; // ItemCache::Stats holds 10 types then 10 values

	// unk_C5D388: the client's item comparison cache.
	static constexpr int COMPARE_CACHE_STRIDE = 0x14C;
	static constexpr int COMPARE_CACHE_SLOTS = 2;

	struct ExtendedStats
	{
		int32 values[CUSTOM_STAT_COUNT];
	};

	// Null when the summary has no custom stats on it.
	const ExtendedStats* Find(const void* summary);
	// Creates a zeroed entry when missing.
	ExtendedStats& Get(const void* summary);
	void Erase(const void* summary);

	// "ITEM_MOD_MAGIC_FIND_SHORT" and friends, indexed the same as customItemModStrings.
	const char* ShortStringKey(int index);
}
