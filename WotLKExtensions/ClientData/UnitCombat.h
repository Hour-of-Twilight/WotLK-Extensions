#pragma once

#include <Defines.h>

#include <cstdint>
#include <ClientData/GameEnums.h>

namespace ClientData::UnitCombat
{
	// The round struct sub_755050 builds from SMSG_ATTACKERSTATEUPDATE and hands to
	// CGUnit_C::PlayAttackerRound.
	struct AttackerRound
	{
		uint32 padding0x00[4];
		uint32 hitInfo;
		uint32 padding0x14[16];
		int32 playSwingFx; // non-zero -> run the swing FX vfunc
		uint32 padding0x58;
		int32 suppressAnim; // non-zero -> skip (re)playing the swing anim
	};

	static_assert(sizeof(AttackerRound) == 0x60, "AttackerRound layout mismatch");

	// First two bytes of an item display record.
	struct EquippedItemRecord
	{
		uint8 itemClass;
		uint8 itemSubclass;
	};

	// Slot indices passed to the get-weapon vfunc.
	enum WeaponSlot : int
	{
		WEAPON_SLOT_MAINHAND = 0,
		WEAPON_SLOT_OFFHAND = 1,
	};

	// CGUnit_C vtable slots, as byte offsets into the vtable.
	constexpr uintptr_t VF_PLAY_SWING_FX = 0x118;     // (int criticalBit, int)
	constexpr uintptr_t VF_GET_WEAPON_RECORD = 0x12C; // (int slot, int) -> item display record

	using GetWeaponRecordFn = EquippedItemRecord*(__thiscall*)(void* self, int slot, int unused);
	using PlaySwingFxFn = void(__thiscall*)(void* self, int criticalBit, int unused);

	inline void* GetVFunc(void* unit, uintptr_t byteOffset)
	{
		uint8* vtable = *reinterpret_cast<uint8**>(unit);
		return *reinterpret_cast<void**>(vtable + byteOffset);
	}

	inline EquippedItemRecord* GetEquippedItem(void* unit, WeaponSlot slot)
	{
		auto getWeapon = reinterpret_cast<GetWeaponRecordFn>(GetVFunc(unit, VF_GET_WEAPON_RECORD));
		return getWeapon(unit, slot, 0);
	}

	// Null unless the slot holds an actual weapon (a shield or held-in-offhand item
	// comes back as ITEM_CLASS_ARMOR).
	inline EquippedItemRecord* GetEquippedWeapon(void* unit, WeaponSlot slot)
	{
		EquippedItemRecord* record = GetEquippedItem(unit, slot);
		return (record && record->itemClass == ITEM_CLASS_WEAPON) ? record : nullptr;
	}

	inline void PlaySwingEffect(void* unit, int criticalBit)
	{
		auto playSwingFx = reinterpret_cast<PlaySwingFxFn>(GetVFunc(unit, VF_PLAY_SWING_FX));
		playSwingFx(unit, criticalBit, 0);
	}
}
