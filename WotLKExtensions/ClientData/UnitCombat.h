#pragma once

#include <cstdint>
#include <ClientData/GameEnums.h>

namespace ClientData::UnitCombat
{
	// The round struct sub_755050 builds from SMSG_ATTACKERSTATEUPDATE and hands to
	// CGUnit_C::PlayAttackerRound.
	struct AttackerRound
	{
		uint32_t padding0x00[4];
		uint32_t hitInfo;
		uint32_t padding0x14[16];
		int32_t playSwingFx;  // non-zero -> run the swing FX vfunc
		uint32_t padding0x58;
		int32_t suppressAnim; // non-zero -> skip (re)playing the swing anim
	};

	static_assert(sizeof(AttackerRound) == 0x60, "AttackerRound layout mismatch");

	// First two bytes of an item display record.
	struct EquippedItemRecord
	{
		uint8_t itemClass;
		uint8_t itemSubclass;
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

	// Model state pointer on CGUnit_C. The counter at +0x48 drops to zero when the
	// unit has no model to animate.
	constexpr uintptr_t UNIT_MODEL_STATE = 0xD0;
	constexpr uintptr_t MODEL_STATE_COUNTER = 0x48;

	using GetWeaponRecordFn = EquippedItemRecord*(__thiscall*)(void* self, int slot, int unused);
	using PlaySwingFxFn = void(__thiscall*)(void* self, int criticalBit, int unused);

	inline void* GetVFunc(void* unit, uintptr_t byteOffset)
	{
		uint8_t* vtable = *reinterpret_cast<uint8_t**>(unit);
		return *reinterpret_cast<void**>(vtable + byteOffset);
	}

	inline bool HasAnimatableModel(void* unit)
	{
		uint8_t* modelState = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(unit) + UNIT_MODEL_STATE);
		return modelState && *reinterpret_cast<int*>(modelState + MODEL_STATE_COUNTER) > 0;
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
