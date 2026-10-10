#pragma once

#include <Defines.h>

#include <cstddef>
#include <cstdint>
#include <ClientData/MathTypes.h>

namespace ClientData
{
	enum Field : uint32
	{
		CURRENT_HP = 18,
		CURRENT_MANA = 19,
		CURRENT_RAGE = 20,
		CURRENT_FOCUS = 21,
		CURRENT_ENERGY = 22,
		CURRENT_HAPPINESS = 23,
		CURRENT_RUNES = 24,
		CURRENT_RUNIC_POWER = 25,
		MAX_HP = 26,
		MAX_MANA = 27,
		MAX_RAGE = 28,
		MAX_FOCUS = 29,
		MAX_ENERGY = 30,
		MAX_HAPPINESS = 31,
		MAX_RUNES = 32,
		MAX_RUNIC_POWER = 33,
	};

	struct MovementInfo
	{
		uint32 padding[2];
		uint64 moverGUID;
		C3Vector position;
		float padding1C;
		float orientation;
		float pitch;
		uint32 padding28[7];
		uint32 movementFlags;
		uint32 movementFlags2;
		C3Vector anchor;
		uint32 padding58[14];
		float walkSpeed;
		uint32 padding94[9];
		float jumpVelocity;
		uint32 paddingBC[37];
		// TODO: add rest, probably when needed
	};

	static_assert(offsetof(MovementInfo, movementFlags) == 0x44, "MovementInfo layout mismatch");
	static_assert(offsetof(MovementInfo, anchor) == 0x4C, "MovementInfo layout mismatch");
	static_assert(offsetof(MovementInfo, walkSpeed) == 0x90, "MovementInfo layout mismatch");
	static_assert(offsetof(MovementInfo, jumpVelocity) == 0xB8, "MovementInfo layout mismatch");
	static_assert(sizeof(MovementInfo) == 0x150, "MovementInfo layout mismatch");

	struct UnitBytes0
	{
		uint8 raceID;
		uint8 classID;
		uint8 genderID;
		uint8 powerTypeID;
	};

	struct UnitBytes1
	{
		uint8 standState;
		uint8 petTalents;
		uint8 visFlags;
		uint8 animTier;
	};

	struct UnitBytes2
	{
		uint8 sheathState;
		uint8 pvpFlag;
		uint8 petFlags;
		uint8 shapeshift;
	};

	struct UnitFields
	{
		uint64 charm;
		uint64 summon;
		uint64 critter;
		uint64 charmedBy;
		uint64 summonedBy;
		uint64 createdBy;
		uint64 target;
		uint64 channelObject;
		uint32 channelSpell;
		UnitBytes0 unitBytes0;
		uint32 unitCurrHealth;
		uint32 unitCurrPowers[7];
		uint32 unitMaxHealth;
		uint32 unitMaxPowers[7];
		float padding0x88[14];
		uint32 level;
		uint32 factionTemplate;
		uint32 virtualItemSlotID[3];
		uint32 unitFlags;
		uint32 unitFlags2;
		uint32 auraState;
		uint32 baseAttackTime[3];
		float boundingRadius;
		float combatReach;
		uint32 displayId;
		uint32 nativeDisplayId;
		uint32 mountDisplayId;
		float mainHandMinDamage;
		float mainHandMaxDamage;
		float offHandMinDamage;
		float offHandMaxDamage;
		UnitBytes1 unitBytes1;
		uint32 petNumber;
		uint32 padding0x011C[4];
		float modCastSpell;
		uint32 createdBySpell;
		uint32 npcFlags;
		uint32 emoteState;
		int32 strength;
		int32 agility;
		int32 stamina;
		int32 intellect;
		int32 spirit;
		float strengthPos;
		float agilityPos;
		float staminaPos;
		float intellectPos;
		float spiritPos;
		float strengthNeg;
		float agilityNeg;
		float staminaNeg;
		float intellectNeg;
		float spiritNeg;
		uint32 resistances[7];
		uint32 resistancesPos[7];
		uint32 resistancesNeg[7];
		uint32 baseMana;
		uint32 basehealth;
		UnitBytes2 unitBytes2;
		uint32 AP;
		uint16 APMods[2];
		float APMult;
		uint32 RAP;
		uint16 RAPMods[2];
		float RAPMult;
		float minRangedDamage;
		float maxRangedDamage;
		uint32 powerCostModifier[7];
		float powerCostMultiplier[7];
		float maxHealthModifier;
		float hoverHeight;
		uint32 padding;
		// TODO: add rest at some point, most likely when needed
	};
}
