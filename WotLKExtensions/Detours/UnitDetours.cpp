#include "UnitDetours.h"
#include <ClientDetours.h>
#include <ClientData/ClientFunctions.h>
#include <SharedDefines.h>
#include <UnitLevelCache.h>

static const uint8_t* GetNameplateLevelColor(int displayLevel, int playerLevel)
{
	static const uint8_t kRed[4] = {0x19, 0x19, 0xFF, 0xFF};
	static const uint8_t kOrange[4] = {0x3F, 0x7F, 0xFF, 0xFF};
	static const uint8_t kYellow[4] = {0x00, 0xFF, 0xFF, 0xFF};
	static const uint8_t kGreen[4] = {0x3F, 0xB2, 0x3F, 0xFF};
	static const uint8_t kGray[4] = {0x7F, 0x7F, 0x7F, 0xFF};

	// Indexed by playerLevel / 5; entries past the table clamp to 8.
	static const int kGrayDiff[20] = {4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8};

	int diff = displayLevel - playerLevel;
	if (diff >= 5)
		return kRed;
	if (diff >= 3)
		return kOrange;
	if (diff >= -2)
		return kYellow;

	int idx = playerLevel / 5;
	int grayDiff = (idx >= 20 || idx < 0) ? 8 : kGrayDiff[idx];
	if ((playerLevel - displayLevel) <= grayDiff)
		return kGreen;
	return kGray;
}

CLIENT_DETOUR_THISCALL(CGNamePlateFrame__UpdateLevelDisplay, 0x0098EF10, void, (void* unit))
{
	if (!self || !unit)
		return;

	CGUnit* cgUnit = static_cast<CGUnit*>(unit);
	CGUnit* cgPlayer = reinterpret_cast<CGUnit*>(ClntObjMgr::GetActivePlayerObj());
	if (!cgPlayer)
		return;

	if (!cgUnit->objectBase.ObjectData || !cgPlayer->objectBase.ObjectData)
		return;

	uint8_t* selfBytes = reinterpret_cast<uint8_t*>(self);
	uint8_t* levelText = *reinterpret_cast<uint8_t**>(selfBytes + 0x2D4);
	uint8_t* skullIcon = *reinterpret_cast<uint8_t**>(selfBytes + 0x2B4);
	if (!levelText || !skullIcon)
		return;

	uint64_t guid = cgUnit->objectBase.ObjectData->OBJECT_FIELD_GUID;
	uint32_t dlvl = sUnitLevelCache.GetUnitItemLevelOrDungeonLevel(guid);
	int displayLevel = dlvl ? static_cast<int>(dlvl)
	                        : (cgUnit->unitData ? static_cast<int>(cgUnit->unitData->level) : 0);

	bool showLevel = !CGUnit_C::IsBossMob(cgUnit);

	if (showLevel)
	{
		levelText[0xCC] |= 0x10;
		CSimpleRegion::ShowThis(levelText);
		skullIcon[0xCC] &= 0xEF;
		CSimpleRegion::HideThis(skullIcon);

		uint64_t playerGuid = cgPlayer->objectBase.ObjectData->OBJECT_FIELD_GUID;
		uint32_t playerIlvl = sUnitLevelCache.GetPlayerItemLevel(playerGuid);
		int playerLevel = playerIlvl ? static_cast<int>(playerIlvl)
		                             : (cgPlayer->unitData ? static_cast<int>(cgPlayer->unitData->level) : 0);
		const uint8_t* color = GetNameplateLevelColor(displayLevel, playerLevel);
		CSimpleRegion::SetVertexColor(levelText, const_cast<uint8_t*>(color));

		char buf[32];
		sprintf_s(buf, "%d", displayLevel);
		CSimpleFontString::SetText(levelText, buf, 1);
	}
	else
	{
		levelText[0xCC] &= 0xEF;
		CSimpleRegion::HideThis(levelText);
		skullIcon[0xCC] |= 0x10;
		CSimpleRegion::ShowThis(skullIcon);
	}
}


static const uint32_t UNIT_FLAG2_REGENERATE_POWER = 0x800;

CLIENT_DETOUR_THISCALL(CGUnit_C__DoPowerRegen, 0x00728A20, void, (int elapsedMs))
{
	CGUnit_C__DoPowerRegen(self, elapsedMs);

	CGUnit* unit = static_cast<CGUnit*>(self);
	if (!unit || !unit->unitData || self != ClntObjMgr::GetActivePlayerObj())
		return;

	UnitFields* fields = unit->unitData;

	if (fields->unitBytes0.powerTypeID == POWER_FOCUS)
		return;

	uint32_t maxFocus = fields->unitMaxPowers[POWER_FOCUS];
	if (maxFocus == 0 || !(fields->unitFlags2 & UNIT_FLAG2_REGENERATE_POWER))
		return;

	if (CGPlayer_C::IsDeadOrGhost(static_cast<CGPlayer*>(self)))
		return;

	double ratePerSecond = CGUnit_C::GetPowerRegen(&unit->unitData, POWER_FOCUS, 0);
	if (ratePerSecond == 0.0)
		return;

	static float sFocusFraction = 0.0f;
	sFocusFraction += static_cast<float>(ratePerSecond) * static_cast<float>(elapsedMs) * 0.001f;

	int whole = static_cast<int>(sFocusFraction);
	if (whole == 0)
		return;
	sFocusFraction -= static_cast<float>(whole);

	int newValue = CGUnit_C::GetPredictedPower(unit, POWER_FOCUS) + whole;
	if (newValue < 0)
		newValue = 0;
	else if (newValue > static_cast<int>(maxFocus))
		newValue = static_cast<int>(maxFocus);

	CGUnit_C::SetPredictedPowerSlot(unit, POWER_FOCUS, newValue);
}

CLIENT_DETOUR(Script_UnitLevel, 0x0060F9E0, __cdecl, int, (lua_State * L))
{
	const char* token = FrameScript::ToLString(L, 1, false);
	if (token)
	{
		uint64_t guid = 0;
		Script_GetGUIDFromToken(token, &guid, 0);
		if (guid)
		{
			uint32_t lvl = sUnitLevelCache.GetUnitItemLevelOrDungeonLevel(guid);
			if (lvl)
			{
				void* unit = ClntObjMgr::ObjectPtr(guid, TYPEMASK_UNIT);
				if (unit && !CGUnit_C::IsBossMob(unit))
				{
					FrameScript::PushNumber(L, static_cast<double>(lvl));
					return 1;
				}
			}
		}
	}
	return Script_UnitLevel(L);
}

static bool GuidWantsLevelCache(uint64_t guid)
{
	if (guid == 0)
		return false;
	uint16_t high = static_cast<uint16_t>(guid >> 48);
	return high == 0x0000 || high == 0xF130 || high == 0xF140 || high == 0xF150;
}

CLIENT_DETOUR(PostInitObject, 0x004D63B0, __cdecl, void*, (CDataStore* createBlock, int isCreate2))
{
	uint64_t guid = 0;
	if (createBlock)
	{
		int32_t savedRead = createBlock->m_read;
		CDataStore_C::GetWowGUID(createBlock, &guid);
		createBlock->m_read = savedRead;
	}

	void* result = PostInitObject(createBlock, isCreate2);

	if (GuidWantsLevelCache(guid))
		sUnitLevelCache.SendRequest(guid);

	return result;
}

int Script_TrueLevel(lua_State* L)
{
	return Script_UnitLevel(L);
}
