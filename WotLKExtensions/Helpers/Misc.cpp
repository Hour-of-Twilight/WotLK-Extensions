#include "Misc.h"

#include <ctime>
#include <UnitLevelCache.h>

void PatchString(BYTE* patchAddr, const char* replacement)
{
	char* pStr = (char*)VirtualAlloc(nullptr, strlen(replacement) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	strcpy(pStr, replacement);

	DWORD oldProtect;
	VirtualProtect(pStr, strlen(replacement) + 1, PAGE_READONLY, &oldProtect);

	VirtualProtect(patchAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
	patchAddr[0] = 0x68; // PUSH imm32
	*reinterpret_cast<char**>(patchAddr + 1) = pStr;
	VirtualProtect(patchAddr, 5, oldProtect, &oldProtect);
}

#include <FrameDetours.h>

void Misc::ApplyPatches()
{
	if (noAmmoPatch)
	{
		uint8_t byteArray[] = { 0xE9, 0xBA, 0x00, 0x00, 0x00 };
		Util::OverwriteBytesAtAddress(0x809540, byteArray, sizeof(byteArray));
	}

	if (extendedItemMods)
	{
		// Code needs to be un-commented when you decide to add some custom mods
		// ofc can replace "ITEM_MOD_TEST" with your own

		memcpy(&itemModTable, (const void*)0xAD6640, 0xC4);
		Util::OverwriteUInt32AtAddress(0x5DC1EB, (uint32_t)&itemModTable);
		Util::OverwriteUInt32AtAddress(0x629211, (uint32_t)&itemModTable);
		Util::OverwriteUInt32AtAddress(0x6292E6, (uint32_t)&itemModTable);
		Util::OverwriteUInt32AtAddress(0x62BC60, (uint32_t)&itemModTable);
		Util::OverwriteUInt32AtAddress(0x62BE67, (uint32_t)&itemModTable);
		//
		Util::OverwriteUInt32AtAddress(0x62BD82, (uint32_t)&itemModTable[45]);
		// when you read code in ida, this table is kinda dumb but what can you do about it
		memcpy(&itemModTableVal, (const void*)0xA25F78, 0x94);
		Util::OverwriteUInt32AtAddress(0x62BC43, (uint32_t)&itemModTableVal);
		Util::SetByteAtAddress((void*)0x62BC4E, (uint8_t)(sizeof(itemModTableVal) / 4));
		Util::OverwriteUInt32AtAddress(0x62BC59, (uint32_t)&itemModTableVal);
		Util::OverwriteUInt32AtAddress(0x62BE4A, (uint32_t)&itemModTableVal);
		Util::SetByteAtAddress((void*)0x62BE55, (uint8_t)(sizeof(itemModTableVal) / 4));
		Util::OverwriteUInt32AtAddress(0x62BE60, (uint32_t)&itemModTableVal);

		for (uint32_t i = 0; i < sizeof(customItemModStrings) / 4; i++)
			itemModTable[i + 49] = (uint32_t)customItemModStrings[i];
		//
		for (uint32_t j = 37; j < sizeof(itemModTableVal) / 4; j++)
			itemModTableVal[j] = j + 12;
	}

	// this one is non-optional, it's WoWTime fix I guess
	// and don't let me get started how retarded original idea behind packing like this is
	// This patches original function to call custom function from dll, otherwise I would need to patch a lot of pointers
	/*uint8_t byteArray[] = {0x8B, 0x55, 0x08, 0x50, 0x52, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x83, 0xC4, 0x08, 0x5D, 0xC3};
	Util::OverwriteBytesAtAddress(0x76CA56, byteArray, sizeof(byteArray));
	Util::OverwriteUInt32AtAddress(0x76CA5C, (uint32_t)&PackWoWTimeToDword - 0x76CA60);
	//
	Util::OverwriteUInt32AtAddress(0x4C989C, (uint32_t)&PackTimeDataToDword - 0x4C98A0);
	Util::OverwriteUInt32AtAddress(0x4C98C2, (uint32_t)&PackTimeDataToDword - 0x4C98C6);
	//
	Util::OverwriteUInt32AtAddress(0x76CAD4, (uint32_t)&UnpackWoWTime - 0x76CAD8);
	Util::OverwriteUInt32AtAddress(0x76CB19, (uint32_t)&UnpackWoWTime - 0x76CB1D);
	Util::OverwriteUInt32AtAddress(0x76CB93, (uint32_t)&UnpackWoWTime - 0x76CB97);
	//
	Util::OverwriteUInt32AtAddress(0x76DA89, (uint32_t)&GetTimeString - 0x76DA8D);
	Util::OverwriteUInt32AtAddress(0x76DA9F, (uint32_t)&GetTimeString - 0x76DAA3);
	Util::OverwriteUInt32AtAddress(0x7E27B7, (uint32_t)&GetTimeString - 0x7E27BB);
	Util::OverwriteUInt32AtAddress(0x7E2AC7, (uint32_t)&GetTimeString - 0x7E2ACB);
	// Patching a bunch of year >= 31 checks
	Util::SetByteAtAddress((void*)0x5B7ACC, 0xEB);
	Util::SetByteAtAddress((void*)0x5B7ACD, 0x08);
	Util::OverwriteBytesAtAddress((void*)0x5B82C0, 0xFF, 0x03);
	Util::SetByteAtAddress((void*)0x5B82C3, 0x7F);
	Util::SetByteAtAddress((void*)0x5B8F35, 0xEB);
	Util::SetByteAtAddress((void*)0x5B8F99, 0xEB);
	Util::SetByteAtAddress((void*)0x5B8F9A, 0x08);
	Util::OverwriteBytesAtAddress((void*)0x5BFF44, 0x90, 0x03);
	Util::SetByteAtAddress((void*)0x5BFF58, 0xEB);
	Util::OverwriteBytesAtAddress((void*)0x5C01F9, 0x90, 0x03);
	Util::OverwriteBytesAtAddress((void*)0x5C01FF, 0x90, 0x02);
	Util::OverwriteBytesAtAddress((void*)0x5C04BF, 0x90, 0x03);
	Util::OverwriteBytesAtAddress((void*)0x5C04C5, 0x90, 0x02);
	Util::SetByteAtAddress((void*)0x5C2890, 0xEB);
	Util::SetByteAtAddress((void*)0x5C2891, 0x08);
	Util::OverwriteBytesAtAddress((void*)0x76C4A8, 0x90, 0x05);*/

	// Util::SetByteAtAddress((void*)0x7FD99C, 0xEB);
	/* {
	    const char* newText = "<Live, Laugh, Toaster Bath>";
	    char* devString = reinterpret_cast<char*>(0x00A3252C); // original string location
	    DWORD oldProtect;
	    if (VirtualProtect(devString, strlen(newText) + 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
	        strcpy_s(devString, strlen(newText) + 1, newText);
	        VirtualProtect(devString, strlen(newText) + 1, oldProtect, &oldProtect);
	    }
	}*/

	// Remove Equip: from item mods and empty space.
	PatchString(reinterpret_cast<BYTE*>(0x0062BF12), "ITEM_PLUS");
	PatchString(reinterpret_cast<BYTE*>(0x0062BF20), "%s%s");
	// Disable action bar removal of spells
	{
		uint32_t callAddr = 0x006E7367;
		uint32_t newFunc = 0x005AA470;

		uint32_t rel = newFunc - (callAddr + 5);

		uint8_t patch[5];
		patch[0] = 0xE8;
		memcpy(&patch[1], &rel, 4);

		Util::OverwriteBytesAtAddress(callAddr, patch, 5);
		uint8 patch2[] = { 0x90, 0x90 };
		Util::OverwriteBytesAtAddress(0x6D880C, patch2, sizeof(patch2));
		uint8 patch3[] = { 0x90, 0x90 };
		Util::OverwriteBytesAtAddress(0x6E8018, patch3, sizeof(patch3));
	}
	// Disable Item Comparsion tooltips.
	{
		uint8_t patch[5] = { 0x31, 0xC0, 0x90, 0x90, 0x90 };

		Util::OverwriteBytesAtAddress(0x00631CB3, patch, 5);

		Util::OverwriteBytesAtAddress(0x00631CED, patch, 5);
	}
	// Allow all spells to be added to the hotbar.
	Util::OverwriteBytesAtAddress((void*)0x542D61, 0x90, 6);
	Util::OverwriteBytesAtAddress((void*)0x542D79, 0x90, 2);

	// CGNamePlateFrame__UpdateLevelDisplay disable level check.
	Util::OverwriteBytesAtAddress((void*)0x0098EFB9, 0x90, 2);

	// Script_UnitLevel disable level check.
	Util::OverwriteBytesAtAddress((void*)0x0060FAB2, 0x90, 2);
	// CGTooltip__SetUnit use our own level grab.
	{
		uint8_t patch[6];
		patch[0] = 0x53; // push ebx  (unit*)
		patch[1] = 0xE8; // call rel32
		uint32_t rel = reinterpret_cast<uint32_t>(&UnitLevelCache::GetTooltipUnitLevel) - (0x006216DB + 5);
		memcpy(&patch[2], &rel, 4);
		Util::OverwriteBytesAtAddress(0x006216DA, patch, sizeof(patch));
	}
	// CGTooltip__SetUnit disable level check.
	Util::OverwriteBytesAtAddress((void*)0x00621766, 0x90, 6);
	// CGSpellBook__AddKnownSpell: redirect the call at 0x00542DE3 (call CGActionBar__PutActionInSlot)
	Util::OverwriteUInt32AtAddress(0x00542DE4, (uint32_t)&CGSpellBook__AddKnownSpell_PutActionInSlotHook - 0x00542DE8);
	// CGSpellBook__AddKnownSpell: NOP the SPELL_ATTR7_SUMMON_PLAYER_TOTEM gate
	Util::OverwriteBytesAtAddress((void*)0x005428B4, 0x90, 6);
}

void Misc::PackTimeDataToDword(uint32_t* packedTime, int32_t minute, int32_t hour, int32_t weekDay, int32_t monthDay, int32_t month, int32_t year, int32_t flags)
{
	uint32_t temp = 0;
	temp += minute & 63;
	temp += (hour & 31) << 6;
	temp += (weekDay & 7) << 11;
	temp += (monthDay & 63) << 14;
	temp += (month & 15) << 20;
	temp += year >= 31 ? 31 << 24 : (year & 31) << 24;
	temp += (flags & 3) << 29;

	*packedTime = temp;
}

void Misc::PackWoWTimeToDword(uint32_t* dword, WoWTime* time)
{
	uint32_t temp = 0;
	temp += time->minute & 63;
	temp += (time->hour & 31) << 6;
	temp += (time->weekDay & 7) << 11;
	temp += (time->monthDay & 63) << 14;
	temp += (time->month & 15) << 20;
	temp += time->year >= 31 ? 31 << 24 : (time->year & 31) << 24;
	temp += (time->flags & 3) << 29;

	*dword = temp;
}

void Misc::UnpackWoWTime(uint32_t packedTime, int32_t* minute, int32_t* hour, int32_t* weekDay, int32_t* monthDay, int32_t* month, int32_t* year, int32_t* flags)
{
	if (minute)
	{
		if ((packedTime & 63) == 63)
			*minute = -1;
		else
			*minute = packedTime & 63;
	}

	if (hour)
	{
		if (((packedTime >> 6) & 31) == 31)
			*hour = -1;
		else
			*hour = (packedTime >> 6) & 31;
	}

	if (weekDay)
	{
		if (((packedTime >> 11) & 7) == 7)
			*weekDay = -1;
		else
			*weekDay = (packedTime >> 11) & 7;
	}

	if (monthDay)
	{
		if (((packedTime >> 14) & 63) == 63)
			*monthDay = -1;
		else
			*monthDay = (packedTime >> 14) & 63;
	}

	if (month)
	{
		if (((packedTime >> 20) & 15) == 15)
			*month = -1;
		else
			*month = (packedTime >> 20) & 15;
	}

	if (year)
		*year = (packedTime >> 24) & 31 + yearOffsetMult * 32;

	if (flags)
	{
		if (((packedTime >> 29) & 3) == 3)
			*flags = -1;
		else
			*flags = (packedTime >> 29) & 3;
	}
}

char* Misc::GetTimeString(WoWTime* a1, char* a2, uint32_t a3)
{
	// Aleist3r: giving up a couple things from original function, originally it was doing a bunch of data & num operations in the first if
	// after that it's checking if values are >= 0...
	if (a1->minute > -1 && a1->hour > -1 && a1->weekDay > -1 && a1->monthDay > -1 && a1->month > -1 && a1->year > -1)
	{
		const char* weekDays[7] = {
			"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
		};

		char month[8];
		char monthDay[8];
		char year[8];
		char weekDay[8];
		char hour[8];
		char minute[8];

		SStr::Printf(month, 8, "%i", a1->month + 1);
		SStr::Printf(monthDay, 8, "%i", a1->monthDay + 1);
		SStr::Printf(year, 8, "%i", a1->year + 2000);
		SStr::Printf(weekDay, 8, "%s", weekDays[a1->weekDay]);
		SStr::Printf(hour, 8, "%i", a1->hour);
		SStr::Printf(minute, 8, "%i", a1->minute);

		SStr::Printf(a2, a3, "%s/%s/%s (%s) %s:%s", month, monthDay, year, weekDay, hour, minute);
	}
	else
		SStr::Printf(a2, a3, "Not Set");

	return a2;
}

void Misc::SetYearOffsetMultiplier()
{
	time_t now = time(0);
	tm* ltm = localtime(&now);

	yearOffsetMult = (ltm->tm_year - 100) / 32;
}

void Misc::ToggleWireframeMode()
{
	uint8_t renderFlags = *(uint8_t*)0xCD774D;
	bool isWireframe = renderFlags & 0x20;
	if (isWireframe)
		*(uint8_t*)0xCD774D = renderFlags & ~0x20;
	else
		*(uint8_t*)0xCD774D = renderFlags | 0x20;
}

void Misc::ToggleDisplayNormals()
{
	uint8_t renderFlags = *(uint8_t*)0xCD774D;
	bool areNormalsDisplayed = renderFlags & 0x10;
	if (areNormalsDisplayed)
		*(uint8_t*)0xCD774D = renderFlags & ~0x10;
	else
		*(uint8_t*)0xCD774D = renderFlags | 0x10;
}

void Misc::ToggleTerrainCulling()
{
	uint8_t renderFlags = *(uint8_t*)0xCD774E;
	bool isTerrainCulled = renderFlags & 0x20;
	if (isTerrainCulled)
		*(uint8_t*)0xCD774E = renderFlags & ~0x20;
	else
		*(uint8_t*)0xCD774E = renderFlags | 0x20;
}

void Misc::ToggleGroundEffects()
{
	uint8_t renderFlags = *(uint8_t*)0xCD774E;
	bool areGroundEffectsDisplayed = renderFlags & 0x10;
	if (areGroundEffectsDisplayed)
		*(uint8_t*)0xCD774E = renderFlags & ~0x10;
	else
		*(uint8_t*)0xCD774E = renderFlags | 0x10;
}

void Misc::ToggleTerrain()
{
	uint8_t renderFlags = *(uint8_t*)0xCD774C;
	bool isTerrainDisplayed = renderFlags & 0x2;
	if (isTerrainDisplayed)
		*(uint8_t*)0xCD774C = renderFlags & ~0x2;
	else
		*(uint8_t*)0xCD774C = renderFlags | 0x2;
}

void Misc::ToggleLiquids()
{
	uint8_t renderFlags = *(uint8_t*)0xCD774C;
	bool areLiquidsDisplayed = renderFlags & 0x4;
	if (areLiquidsDisplayed)
		*(uint8_t*)0xCD774C = renderFlags & ~0x4;
	else
		*(uint8_t*)0xCD774C = renderFlags | 0x4;
}

void Misc::ToggleM2()
{
	uint8_t renderFlags = *(uint8_t*)0xCD774C;
	bool areM2Displayed = renderFlags & 0x1;
	if (areM2Displayed)
		*(uint8_t*)0xCD774C = renderFlags & ~0x1;
	else
		*(uint8_t*)0xCD774C = renderFlags | 0x1;
}

void Misc::ToggleWMO()
{
	uint8_t renderFlags = *(uint8_t*)0xCD774D;
	bool areWMOsDisplayed = renderFlags & 0x1;
	if (areWMOsDisplayed)
		*(uint8_t*)0xCD774D = renderFlags & ~0x1;
	else
		*(uint8_t*)0xCD774D = renderFlags | 0x1;
}
