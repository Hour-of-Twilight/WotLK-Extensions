#include "ItemStatsSummary.h"

#include <ClientDetours.h>
#include <ClientData/ClientAddresses.h>
#include <ClientData/ClientFunctions.h>

#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>

using namespace ItemStatsSummary;

static bool sInComparison = false;
static bool sSubCaptured = false;
static ExtendedStats sCapturedDelta = {};

static std::unordered_map<const void*, ExtendedStats>& SideStats()
{
	static std::unordered_map<const void*, ExtendedStats> stats;
	return stats;
}

static bool IsCustomStat(int type)
{
	return type >= CUSTOM_STAT_FIRST && type <= CUSTOM_STAT_LAST;
}

static void AddCustomStat(void* summary, int type, int value)
{
	ItemStatsSummary::Get(summary).values[type - CUSTOM_STAT_FIRST] += value;
}

const ExtendedStats* ItemStatsSummary::Find(const void* summary)
{
	auto& stats = SideStats();
	auto it = stats.find(summary);
	return it != stats.end() ? &it->second : nullptr;
}

ExtendedStats& ItemStatsSummary::Get(const void* summary)
{
	return SideStats()[summary];
}

void ItemStatsSummary::Erase(const void* summary)
{
	SideStats().erase(summary);
}

const char* ItemStatsSummary::ShortStringKey(int index)
{
	static std::string keys[CUSTOM_STAT_COUNT];
	static bool built = false;
	if (!built)
	{
		for (int i = 0; i < CUSTOM_STAT_COUNT; ++i)
			keys[i] = std::string(customItemModStrings[i]) + "_SHORT";
		built = true;
	}

	if (index < 0 || index >= CUSTOM_STAT_COUNT)
		return "";

	return keys[index].c_str();
}

CLIENT_DETOUR_THISCALL_NOARGS(CGItemStatsSummary__Clear, 0x0061B5B0, void)
{
	ItemStatsSummary::Erase(self);
	CGItemStatsSummary__Clear(self);
}

CLIENT_DETOUR_THISCALL(CGItemStatsSummary__Create, 0x006207A0, int, (char* itemLink))
{
	ItemStatsSummary::Erase(self);
	return CGItemStatsSummary__Create(self, itemLink);
}

CLIENT_DETOUR_THISCALL(CGItemStatsSummary__CreateFromTooltip, 0x00630C70, int, (void* tooltip))
{
	ItemStatsSummary::Erase(self);
	return CGItemStatsSummary__CreateFromTooltip(self, tooltip);
}

static void __cdecl AddItemStatsImpl(void* self, const ItemCache* item)
{
	uint8* summary = static_cast<uint8*>(self);
	*reinterpret_cast<uint32*>(summary + 0x128) |= 0x20;

	if (!item)
		return;

	for (int i = 0; i < item->StatsCount && i < ITEM_CACHE_STAT_COUNT; ++i)
	{
		const int type = item->Stats[i];
		const int value = item->Stats[i + ITEM_CACHE_STAT_COUNT];

		if (IsCustomStat(type))
		{
			AddCustomStat(self, type, value);
			continue;
		}

		if (type < 0 || type > VANILLA_STAT_LAST)
			continue;

		*reinterpret_cast<int32*>(summary + 0x30 + type * 4) += value;
		if (type == ITEM_MOD_ATTACK_POWER)
			*reinterpret_cast<int32*>(summary + 0x24) += value;
		if (type == ITEM_MOD_ATTACK_POWER || type == ITEM_MOD_RANGED_ATTACK_POWER)
			*reinterpret_cast<int32*>(summary + 0x28) += value;
	}
}

__declspec(naked) static void CGItemStatsSummary__AddItemStats_Hook()
{
	__asm {
        push ebp
        mov  ebp, esp
        push ecx
        push dword ptr [ebp+8] // item
        push ecx // self
        call AddItemStatsImpl
        add  esp, 8
        pop  ecx
        mov  esp, ebp
        pop  ebp
        ret  4
	}
}

int CGItemStatsSummary__AddItemStats__Result = ClientDetours::Add("CGItemStatsSummary::AddItemStats",
    &CGItemStatsSummary::AddItemStats, CGItemStatsSummary__AddItemStats_Hook, __FILE__, __LINE__);

CLIENT_DETOUR_THISCALL(CGItemStatsSummary__AddEnchantEffect, 0x0061F2B0, void, (int effect, int arg, int value))
{
	if (effect == 5)
	{
		if (IsCustomStat(arg))
		{
			AddCustomStat(self, arg, value);
			return;
		}

		if (arg < 0 || arg > VANILLA_STAT_LAST)
			return;
	}
	else if (effect == 4) // raw array index, same missing bound as above
	{
		if (arg < 0 || arg >= SUMMARY_ARRAY_COUNT)
			return;
	}

	CGItemStatsSummary__AddEnchantEffect(self, effect, arg, value);
}

CLIENT_DETOUR_THISCALL(CGItemStatsSummary__Subtract, 0x0061DBC0, void*, (void* out, const void* rhs))
{
	void* result = CGItemStatsSummary__Subtract(self, out, rhs);

	const ExtendedStats* lhsStats = ItemStatsSummary::Find(self);
	const ExtendedStats* rhsStats = ItemStatsSummary::Find(rhs);

	ExtendedStats delta = {};
	bool any = false;
	for (int i = 0; i < CUSTOM_STAT_COUNT; ++i)
	{
		delta.values[i] = (lhsStats ? lhsStats->values[i] : 0) - (rhsStats ? rhsStats->values[i] : 0);
		any = any || delta.values[i] != 0;
	}

	if (any)
		ItemStatsSummary::Get(out) = delta;
	else
		ItemStatsSummary::Erase(out);

	if (sInComparison)
	{
		sCapturedDelta = delta;
		sSubCaptured = true;
	}

	return result;
}

// The client shuttles summaries through two global cache slots with inline rep movsd, which the
// side storage cannot follow, so bracket the call and re-attach the delta afterwards.
CLIENT_DETOUR(CGItemStatsSummary__GetItemComparison, 0x00631590, __cdecl, int, (void* tooltipA, void* tooltipB, void* out))
{
	sInComparison = true;
	sSubCaptured = false;

	const int result = CGItemStatsSummary__GetItemComparison(tooltipA, tooltipB, out);

	sInComparison = false;

	if (!result)
		return result;

	if (sSubCaptured)
	{
		// Cache miss: the slot index was stored just before the subtraction ran.
		const int index = *dword_AD2EBC;
		if (index >= 0 && index < COMPARE_CACHE_SLOTS)
			ItemStatsSummary::Get(unk_C5D388 + index * COMPARE_CACHE_STRIDE) = sCapturedDelta;
		ItemStatsSummary::Get(out) = sCapturedDelta;
		return result;
	}

	// Cache hit: out is a byte copy of whichever slot matched.
	ItemStatsSummary::Erase(out);
	for (int i = 0; i < COMPARE_CACHE_SLOTS; ++i)
	{
		const uint8* slot = unk_C5D388 + i * COMPARE_CACHE_STRIDE;
		if (std::memcmp(slot, out, SUMMARY_SIZE) != 0)
			continue;

		if (const ExtendedStats* cached = ItemStatsSummary::Find(slot))
			ItemStatsSummary::Get(out) = *cached;
		break;
	}

	return result;
}

static bool DeltaHeaderConsumed(const void* summary)
{
	if (std::fabs(*reinterpret_cast<const float*>(summary)) > *flt_9EA624)
		return true;

	const int32* array = reinterpret_cast<const int32*>(static_cast<const uint8*>(summary) + 4);
	for (int i = 0; i < SUMMARY_ARRAY_COUNT; ++i)
	{
		if (i == 0x31 || i == 0x32) // the two indices the client's loop skips
			continue;
		if (array[i] != 0)
			return true;
	}

	return false;
}

CLIENT_DETOUR_THISCALL(CGTooltip__SetItemDelta, 0x0062D930, int, (const void* summary, int keepLines))
{
	const int result = CGTooltip__SetItemDelta(self, summary, keepLines);

	const ExtendedStats* custom = ItemStatsSummary::Find(summary);
	if (!custom)
		return result;

	int headerFlag = DeltaHeaderConsumed(summary) ? 0 : 1;
	for (int i = 0; i < CUSTOM_STAT_COUNT; ++i)
	{
		if (custom->values[i] == 0)
			continue;

		CGTooltip::AddColoredItemStat(self, custom->values[i],
		    FrameScript::GetText(ItemStatsSummary::ShortStringKey(i), -1, 0), &headerFlag);
	}

	return result;
}

static void __cdecl AppendCustomStats(const void* summary, lua_State* L)
{
	const ExtendedStats* custom = ItemStatsSummary::Find(summary);
	if (!custom || !L)
		return;

	for (int i = 0; i < CUSTOM_STAT_COUNT; ++i)
	{
		if (custom->values[i] == 0)
			continue;

		FrameScript::PushString(L, ItemStatsSummary::ShortStringKey(i));
		FrameScript::PushNumber(L, custom->values[i]);
		FrameScript::SetTable(L, -3);
	}
}

__declspec(naked) static void PushStatSummary_Hook()
{
	__asm {
        push ebp
        mov  ebp, esp
        push ebx
        mov  ebx, eax // summary
        push dword ptr [ebp+8] // lua_State*
        mov  eax, ebx
        call CGItemStatsSummary::PushStatSummary
        add  esp, 4
        push dword ptr [ebp+8]
        push ebx
        call AppendCustomStats
        add  esp, 8
        mov  eax, 1
        pop  ebx
        pop  ebp
        ret
	}
}

int PushStatSummary__Result = ClientDetours::Add("CGItemStatsSummary::PushStatSummary",
    &CGItemStatsSummary::PushStatSummary, PushStatSummary_Hook, __FILE__, __LINE__);
