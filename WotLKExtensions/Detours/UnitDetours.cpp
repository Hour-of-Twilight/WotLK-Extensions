#include <Defines.h>
#include "UnitDetours.h"
#include <ClientDetours.h>
#include <ClientData/Animation.h>
#include <ClientData/ClientFunctions.h>
#include <ClientData/UnitCombat.h>
#include <ClientData/WowObjects.h>
#include <SharedDefines.h>
#include <UnitLevelCache.h>
#include <Character/AnimationFixes.h>
#include <cstdlib>

using namespace ClientData::UnitCombat;
using ClientData::WowPlayer;
using ClientData::WowUnit;

static const uint8* GetNameplateLevelColor(int displayLevel, int playerLevel)
{
	static const uint8 kRed[4] = { 0x19, 0x19, 0xFF, 0xFF };
	static const uint8 kOrange[4] = { 0x3F, 0x7F, 0xFF, 0xFF };
	static const uint8 kYellow[4] = { 0x00, 0xFF, 0xFF, 0xFF };
	static const uint8 kGreen[4] = { 0x3F, 0xB2, 0x3F, 0xFF };
	static const uint8 kGray[4] = { 0x7F, 0x7F, 0x7F, 0xFF };

	// Indexed by playerLevel / 5; entries past the table clamp to 8.
	static const int kGrayDiff[20] = { 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8 };

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

	WowUnit target(unit);
	WowPlayer player = WowPlayer::Active();
	if (!target || !player)
		return;

	uint8* selfBytes = reinterpret_cast<uint8*>(self);
	uint8* levelText = *reinterpret_cast<uint8**>(selfBytes + 0x2D4);
	uint8* skullIcon = *reinterpret_cast<uint8**>(selfBytes + 0x2B4);
	if (!levelText || !skullIcon)
		return;

	uint64 guid = target.Guid();
	uint32 dlvl = sUnitLevelCache.GetUnitItemLevelOrDungeonLevel(guid);
	if (!dlvl)
		UnitLevelCache::EnsureRequested(guid);
	int displayLevel = static_cast<int>(dlvl ? dlvl : target.Level());

	bool showLevel = !target.IsBoss();

	if (showLevel)
	{
		levelText[0xCC] |= 0x10;
		CSimpleRegion::ShowThis(levelText);
		skullIcon[0xCC] &= 0xEF;
		CSimpleRegion::HideThis(skullIcon);

		uint64 playerGuid = player.Guid();
		uint32 playerIlvl = sUnitLevelCache.GetPlayerItemLevel(playerGuid);
		if (!playerIlvl)
			UnitLevelCache::EnsureRequested(playerGuid);
		int playerLevel = static_cast<int>(playerIlvl ? playerIlvl : player.Level());
		const uint8* color = GetNameplateLevelColor(displayLevel, playerLevel);
		CSimpleRegion::SetVertexColor(levelText, const_cast<uint8*>(color));

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

CLIENT_DETOUR_THISCALL(CGUnit_C__DoPowerRegen, 0x00728A20, void, (int elapsedMs))
{
	CGUnit_C__DoPowerRegen(self, elapsedMs);

	WowPlayer player(self);
	if (!player || self != ClntObjMgr::GetActivePlayerObj())
		return;

	if (player.PowerType() == POWER_FOCUS)
		return;

	uint32 maxFocus = player.MaxPower(POWER_FOCUS);
	if (maxFocus == 0 || !player.HasUnitFlag2(UNIT_FLAG2_REGENERATE_POWER))
		return;

	if (player.IsDeadOrGhost())
		return;

	double ratePerSecond = CGUnit_C::GetPowerRegen(&player.Unit()->unitData, POWER_FOCUS, 0);
	if (ratePerSecond == 0.0)
		return;

	static float sFocusFraction = 0.0f;
	sFocusFraction += static_cast<float>(ratePerSecond) * static_cast<float>(elapsedMs) * 0.001f;

	int whole = static_cast<int>(sFocusFraction);
	if (whole == 0)
		return;
	sFocusFraction -= static_cast<float>(whole);

	int newValue = player.PredictedPower(POWER_FOCUS) + whole;
	if (newValue < 0)
		newValue = 0;
	else if (newValue > static_cast<int>(maxFocus))
		newValue = static_cast<int>(maxFocus);

	CGUnit_C::SetPredictedPowerSlot(player.Unit(), POWER_FOCUS, newValue);
}

CLIENT_DETOUR_THISCALL(CGUnit_C__PlayAttackerRound, 0x00755130, void, (void* round))
{
	if (!WowUnit(self).IsAlive())
		return;

	AttackerRound* r = static_cast<AttackerRound*>(round);
	if (r->hitInfo & HITINFO_NO_ANIMATION)
		return;

	int animationId = ANIMATION_ATTACK_UNARMED;

	if (r->hitInfo & HITINFO_OFFHAND)
	{
		EquippedItemRecord* offhand = GetEquippedWeapon(self, WEAPON_SLOT_OFFHAND);
		if (offhand)
			animationId = (offhand->itemSubclass == ITEM_SUBCLASS_WEAPON_DAGGER) ? ANIMATION_ATTACK_OFFHAND_PIERCE
			                                                                     : ANIMATION_ATTACK_OFFHAND;
		else if (AnimationFixes::ShouldUseUnarmedAnimations(self))
			animationId = ANIMATION_MONK_ATTACK_OFFHAND;
		else
			animationId = ANIMATION_ATTACK_UNARMED_OFFHAND;
	}
	else
	{
		EquippedItemRecord* mainhand = GetEquippedWeapon(self, WEAPON_SLOT_MAINHAND);
		if (mainhand)
		{
			// Two-handers swing ATTACK_2H, unless the off-hand is also equipped
			// (Titan's Grip), in which case they fall back to the 1H swing.
			bool dualWield = GetEquippedItem(self, WEAPON_SLOT_OFFHAND) != nullptr;
			int twoHandAnim = dualWield ? ANIMATION_ATTACK_1H : ANIMATION_ATTACK_2H;

			switch (mainhand->itemSubclass)
			{
			case ITEM_SUBCLASS_WEAPON_AXE:
			case ITEM_SUBCLASS_WEAPON_MACE:
			case ITEM_SUBCLASS_WEAPON_SWORD:
			case ITEM_SUBCLASS_WEAPON_EXOTIC:
			case ITEM_SUBCLASS_WEAPON_MISC:
				animationId = ANIMATION_ATTACK_1H;
				break;
			case ITEM_SUBCLASS_WEAPON_AXE2:
			case ITEM_SUBCLASS_WEAPON_MACE2:
			case ITEM_SUBCLASS_WEAPON_SWORD2:
			case ITEM_SUBCLASS_WEAPON_EXOTIC2:
				animationId = twoHandAnim;
				break;
			case ITEM_SUBCLASS_WEAPON_DAGGER:
				animationId = ANIMATION_ATTACK_1H_PIERCE;
				break;
			case ITEM_SUBCLASS_WEAPON_POLEARM:
			case ITEM_SUBCLASS_WEAPON_STAFF:
			case ITEM_SUBCLASS_WEAPON_SPEAR:
			case ITEM_SUBCLASS_WEAPON_FISHING_POLE:
				animationId = ANIMATION_ATTACK_2H_LOOSE;
				break;
			default:
				// Bow, gun, fist, thrown, crossbow, wand, obsolete: hold the ranged pose.
				animationId = ANIMATION_READY_RANGED_HOLD;
				break;
			}
		}
		else if (AnimationFixes::ShouldUseUnarmedAnimations(self))
		{
			animationId = (rand() % 100 < 30) ? ANIMATION_MONK_ATTACK_OFFHAND
			                                  : ANIMATION_MONK_ATTACK_MAINHAND;
		}
	}

	if (!r->suppressAnim)
		CGUnit_C::AnimationData(reinterpret_cast<CGUnit*>(self), animationId, 0);

	if (r->playSwingFx)
		PlaySwingEffect(self, (r->hitInfo & HITINFO_CRITICAL_HIT) ? 1 : 0);
}

CLIENT_DETOUR_THISCALL_NOARGS(CGUnit_C__GetReadyWeaponStance, 0x00714DD0, int)
{
	EquippedItemRecord* mainhand = GetEquippedWeapon(self, WEAPON_SLOT_MAINHAND);

	if (mainhand && IsRangedWeaponSubclass(mainhand->itemSubclass))
		return ANIMATION_READY_RANGED_HOLD;

	EquippedItemRecord* weapon = mainhand ? mainhand : GetEquippedWeapon(self, WEAPON_SLOT_OFFHAND);
	if (!weapon)
	{
		if (AnimationFixes::ShouldUseUnarmedAnimations(self))
			return ANIMATION_MONK_READY;
		return ANIMATION_READY_UNARMED;
	}

	bool dualWield = GetEquippedItem(self, WEAPON_SLOT_OFFHAND) != nullptr;
	int twoHandStance = dualWield ? ANIMATION_READY_1H : ANIMATION_READY_2H;

	switch (weapon->itemSubclass)
	{
	case ITEM_SUBCLASS_WEAPON_AXE:
	case ITEM_SUBCLASS_WEAPON_MACE:
	case ITEM_SUBCLASS_WEAPON_SWORD:
	case ITEM_SUBCLASS_WEAPON_EXOTIC:
	case ITEM_SUBCLASS_WEAPON_FIST:
	case ITEM_SUBCLASS_WEAPON_MISC:
	case ITEM_SUBCLASS_WEAPON_DAGGER:
	case ITEM_SUBCLASS_WEAPON_FISHING_POLE:
		return ANIMATION_READY_1H;
	case ITEM_SUBCLASS_WEAPON_AXE2:
	case ITEM_SUBCLASS_WEAPON_MACE2:
	case ITEM_SUBCLASS_WEAPON_SWORD2:
	case ITEM_SUBCLASS_WEAPON_EXOTIC2:
		return twoHandStance;
	case ITEM_SUBCLASS_WEAPON_POLEARM:
	case ITEM_SUBCLASS_WEAPON_STAFF:
	case ITEM_SUBCLASS_WEAPON_SPEAR:
		return ANIMATION_READY_2H_LOOSE;
	default:
		// Bow, gun, crossbow, thrown, wand, obsolete.
		return ANIMATION_READY_UNARMED;
	}
}

CLIENT_DETOUR_THISCALL(CGUnit_C__DestroyUnitMount, 0x0073D940, void, (int updateCollision))
{
	WowUnit unit(self);
	bool hadMount = unit.MountModel() != nullptr;

	CGUnit_C__DestroyUnitMount(self, updateCollision);

	if (!hadMount)
		return;

	int32& mountDisplayId = unit.CachedMountDisplayId();
	int32 savedDisplayId = mountDisplayId;
	mountDisplayId = 0;
	CGUnit_C::UpdateSelectionRadius(self);
	mountDisplayId = savedDisplayId;
}

CLIENT_DETOUR(Script_UnitLevel, 0x0060F9E0, __cdecl, int, (lua_State * L))
{
	const char* token = FrameScript::ToLString(L, 1, false);
	if (token)
	{
		uint64 guid = 0;
		Script_GetGUIDFromToken(token, &guid, 0);
		if (guid)
		{
			uint32 lvl = sUnitLevelCache.GetUnitItemLevelOrDungeonLevel(guid);
			if (!lvl)
				UnitLevelCache::EnsureRequested(guid);
			else
			{
				WowUnit unit = WowUnit::FromGuid(guid);
				if (unit && !unit.IsBoss())
				{
					FrameScript::PushNumber(L, static_cast<double>(lvl));
					return 1;
				}
			}
		}
	}
	return Script_UnitLevel(L);
}

CLIENT_DETOUR(CGUnit_C__GetDisplayClassName, 0x0072AAB0, __cdecl, const char*, (CGUnit * unit, void* nameCacheRecord))
{
	static constexpr uintptr_t kChrClassesDB = 0x00AD3404 + 0x18;

	if (WowUnit target{ unit })
	{
		uint64 guid = target.Guid();
		uint8 subClass = sUnitLevelCache.GetPlayerSubClass(guid);
		if (!subClass)
			UnitLevelCache::EnsureRequested(guid);
		else
		{
			void* row = ClientDB::GetRow(reinterpret_cast<void*>(kChrClassesDB), subClass);
			if (row)
				return CGUnit_C::GetDisplayClassNameFromRecord(row, target.Gender(), nullptr);
		}
	}

	return CGUnit_C__GetDisplayClassName(unit, nameCacheRecord);
}

CLIENT_DETOUR(PostInitObject, 0x004D63B0, __cdecl, void*, (CDataStore * createBlock, int isCreate2))
{
	uint64 guid = 0;
	if (createBlock)
	{
		int32 savedRead = createBlock->m_read;
		CDataStore_C::GetWowGUID(createBlock, &guid);
		createBlock->m_read = savedRead;
	}

	void* result = PostInitObject(createBlock, isCreate2);

	// Unconditional: the object was just (re)created, so any value we still hold for this GUID
	// could belong to a different unit or to stale gear.
	UnitLevelCache::SendRequest(guid);

	return result;
}

int Script_TrueLevel(lua_State* L)
{
	return Script_UnitLevel(L);
}
