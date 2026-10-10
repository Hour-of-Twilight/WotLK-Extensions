#pragma once

#include <Defines.h>

#include <Macros.h>

#include <cstdint>

namespace ClientData
{
	struct SpellRow
	{
		uint32 m_ID;
		uint32 m_category;
		uint32 m_dispelType;
		uint32 m_mechanic;
		uint32 m_attributes;
		uint32 m_attributesEx;
		uint32 m_attributesExB;
		uint32 m_attributesExC;
		uint32 m_attributesExD;
		uint32 m_attributesExE;
		uint32 m_attributesExF;
		uint32 m_attributesExG;
		uint32 m_shapeshiftMask[2];
		uint32 m_shapeshiftExclude[2];
		uint32 m_targets;
		uint32 m_targetCreatureType;
		uint32 m_requiresSpellFocus;
		uint32 m_facingCasterFlags;
		uint32 m_casterAuraState;
		uint32 m_targetAuraState;
		uint32 m_excludeCasterAuraState;
		uint32 m_excludeTargetAuraState;
		uint32 m_casterAuraSpell;
		uint32 m_targetAuraSpell;
		uint32 m_excludeCasterAuraSpell;
		uint32 m_excludeTargetAuraSpell;
		uint32 m_castingTimeIndex;
		uint32 m_recoveryTime;
		uint32 m_categoryRecoveryTime;
		uint32 m_interruptFlags;
		uint32 m_auraInterruptFlags;
		uint32 m_channelInterruptFlags;
		uint32 m_procTypeMask;
		uint32 m_procChance;
		uint32 m_procCharges;
		uint32 m_maxLevel;
		uint32 m_baseLevel;
		uint32 m_spellLevel;
		uint32 m_durationIndex;
		uint32 m_powerType;
		uint32 m_manaCost;
		uint32 m_manaCostPerLevel;
		uint32 m_manaPerSecond;
		uint32 m_manaPerSecondPerLevel;
		uint32 m_rangeIndex;
		float m_speed;
		uint32 m_modalNextSpell;
		uint32 m_cumulativeAura;
		uint32 m_totem[2];
		uint32 m_reagent[8];
		uint32 m_reagentCount[8];
		uint32 m_equippedItemClass;
		uint32 m_equippedItemSubclass;
		uint32 m_equippedItemInvTypes;
		uint32 m_effect[3];
		uint32 m_effectDieSides[3];
		float m_effectRealPointsPerLevel[3];
		uint32 m_effectBasePoints[3];
		uint32 m_effectMechanic[3];
		uint32 m_implicitTargetA[3];
		uint32 m_implicitTargetB[3];
		uint32 m_effectRadiusIndex[3];
		uint32 m_effectAura[3];
		uint32 m_effectAuraPeriod[3];
		float m_effectAmplitude[3];
		uint32 m_effectChainTargets[3];
		uint32 m_effectItemType[3];
		uint32 m_effectMiscValue[3];
		uint32 m_effectMiscValueB[3];
		uint32 m_effectTriggerSpell[3];
		float m_effectPointsPerCombo[3];
		uint32 m_effectSpellClassMaskA[3];
		uint32 m_effectSpellClassMaskB[3];
		uint32 m_effectSpellClassMaskC[3];
		uint32 m_spellVisualID[2];
		uint32 m_spellIconID;
		uint32 m_activeIconID;
		uint32 m_spellPriority;
		char* m_name_lang;
		char* m_nameSubtext_lang;
		char* m_description_lang;
		char* m_auraDescription_lang;
		uint32 m_manaCostPct;
		uint32 m_startRecoveryCategory;
		uint32 m_startRecoveryTime;
		uint32 m_maxTargetLevel;
		uint32 m_spellClassSet;
		uint32 m_spellClassMask[3];
		uint32 m_maxTargets;
		uint32 m_defenseType;
		uint32 m_preventionType;
		uint32 m_stanceBarOrder;
		float m_effectChainAmplitude[3];
		uint32 m_minFactionID;
		uint32 m_minReputation;
		uint32 m_requiredAuraVision;
		uint32 m_requiredTotemCategoryID[2];
		uint32 m_requiredAreasID;
		uint32 m_schoolMask;
		uint32 m_runeCostID;
		uint32 m_spellMissileID;
		uint32 m_powerDisplayID;
		float m_effectBonusCoefficient[3];
		uint32 m_descriptionVariablesID;
		uint32 m_difficulty;
	};

	struct AuraData
	{
		uint64 creator;
		uint32 spellId;
		uint8 flags;
		uint8 level;
		uint8 stackCount;
		uint8 ukn;
		int32 duration;
		int32 endTime;
	};

	struct PendingSpellCastData
	{
		char pad0[0x18];
		uint32 spellId;
	};

	struct PendingSpellCast
	{
		char pad0[0x8];
		PendingSpellCastData data;
	};

	enum SpellAttr0 : uint32
	{
		SPELL_ATTR0_REQ_AMMO = 0x00000002,
		SPELL_ATTR0_ON_NEXT_SWING = 0x00000004,
		SPELL_ATTR0_ABILITY = 0x00000010,
		SPELL_ATTR0_TRADESPELL = 0x00000020,
		SPELL_ATTR0_PASSIVE = 0x00000040,
		SPELL_ATTR0_HIDDEN_CLIENTSIDE = 0x00000080,
		SPELL_ATTR0_ON_NEXT_SWING_2 = 0x00000400,
	};

	enum SpellAttr1 : uint32
	{
		SPELL_ATTR1_CHANNELED_1 = 0x00000004,
		SPELL_ATTR1_CHANNELED_2 = 0x00000040,
	};

	enum SpellAttr2 : uint32
	{
		SPELL_ATTR2_AUTOREPEAT_FLAG = 0x00000020,
	};

	enum SpellAttr3 : uint32
	{
		SPELL_ATTR3_MAIN_HAND = 0x00000400,
		SPELL_ATTR3_REQ_OFFHAND = 0x01000000,
	};

	enum SpellAttr0Cu : uint32
	{
		SPELL_ATTR0_CU_ENCHANT_PROC = 0x00000001,
		SPELL_ATTR0_CU_CONE_BACK = 0x00000002,
		SPELL_ATTR0_CU_CONE_LINE = 0x00000004,
		SPELL_ATTR0_CU_SHARE_DAMAGE = 0x00000008,
		SPELL_ATTR0_CU_NO_INITIAL_THREAT = 0x00000010,
		SPELL_ATTR0_CU_AURA_CC = 0x00000020,
		SPELL_ATTR0_CU_DONT_BREAK_STEALTH = 0x00000040,
		SPELL_ATTR0_CU_CAN_CRIT = 0x00000080,
		SPELL_ATTR0_CU_DIRECT_DAMAGE = 0x00000100,
		SPELL_ATTR0_CU_CHARGE = 0x00000200,
		SPELL_ATTR0_CU_PICKPOCKET = 0x00000400,
		SPELL_ATTR0_CU_ROLLING_PERIODIC = 0x00000800,
		SPELL_ATTR0_CU_NEGATIVE_EFF0 = 0x00001000,
		SPELL_ATTR0_CU_NEGATIVE_EFF1 = 0x00002000,
		SPELL_ATTR0_CU_NEGATIVE_EFF2 = 0x00004000,
		SPELL_ATTR0_CU_IGNORE_ARMOR = 0x00008000,
		SPELL_ATTR0_CU_REQ_TARGET_FACING_CASTER = 0x00010000,
		SPELL_ATTR0_CU_REQ_CASTER_BEHIND_TARGET = 0x00020000,
		SPELL_ATTR0_CU_ALLOW_INFLIGHT_TARGET = 0x00040000,
		SPELL_ATTR0_CU_NEEDS_AMMO_DATA = 0x00080000,
		SPELL_ATTR0_CU_BINARY_SPELL = 0x00100000,
		SPELL_ATTR0_CU_SCHOOLMASK_NORMAL_WITH_MAGIC = 0x00200000,
		SPELL_ATTR0_CU_DEPRECATED_LIQUID_AURA = 0x00400000, // DO NOT REUSE
		SPELL_ATTR0_CU_IS_TALENT = 0x00800000,              // reserved for master branch
		SPELL_ATTR0_CU_AURA_CANNOT_BE_SAVED = 0x01000000,
		SPELL_ATTR0_CU_DONT_RESTART_P_TIMER = 0x02000000,
		SPELL_ATTR0_CU_USE_RANGED_NO_AMMO = 0x04000000,
		SPELL_ATTR0_CU_UNUSED27 = 0x08000000,
		SPELL_ATTR0_CU_UNUSED28 = 0x10000000,
		SPELL_ATTR0_CU_UNUSED29 = 0x20000000,
		SPELL_ATTR0_CU_UNUSED30 = 0x40000000,
		SPELL_ATTR0_CU_BYPASS_MECHANIC_IMMUNITY = 0x80000000,

		SPELL_ATTR0_CU_NEGATIVE = SPELL_ATTR0_CU_NEGATIVE_EFF0 | SPELL_ATTR0_CU_NEGATIVE_EFF1 | SPELL_ATTR0_CU_NEGATIVE_EFF2,
	};

	enum SpellAttr1Cu : uint32
	{
		SPELL_ATTR1_CU_REQUIRES_COMBAT = 0x00000001,
		SPELL_ATTR1_CU_NO_ATTACK_BLOCK = 0x00000002,
		SPELL_ATTR1_CU_SCALE_DAMAGE_EFFECTS_ONLY = 0x00000004,
		SPELL_ATTR1_CU_SCALE_HEALING_EFFECTS_ONLY = 0x00000008,
		SPELL_ATTR1_CU_USE_TARGETS_LEVEL_FOR_SPELL_SCALING = 0x00000010,
		SPELL_ATTR1_CU_REMOVE_OUTSIDE_DUNGEONS_AND_RAIDS = 0x00000020,
		SPELL_ATTR1_CU_NOT_USABLE_IN_INSTANCES = 0x00000040,
		SPELL_ATTR1_CU_USABLE_IN_INSTANCES_ONLY = 0x00000080,
		SPELL_ATTR1_CU_PERIODIC_CAN_CRIT = 0x00000100,
		SPELL_ATTR1_CU_IGNORES_CASTER_LEVEL = 0x00000200,
		SPELL_ATTR1_CU_ONLY_PROC_FROM_CLASS_ABILITIES = 0x00000400,
		SPELL_ATTR1_CU_ACTIVATES_REQUIRED_SHAPESHIFT = 0x00000800,
		SPELL_ATTR1_CU_REAPPLY_NO_REFRESH_DURATION = 0x00001000,
		SPELL_ATTR1_CU_SPECIAL_DELAY_CALCULATION = 0x00002000,
		SPELL_ATTR1_CU_CAST_TIME_UNAFFECTED_BY_HASTE = 0x00004000,
		SPELL_ATTR1_CU_LOW_CAST_TIME_OK = 0x00008000,
		SPELL_ATTR1_CU_STACKS_DONT_ADDUP = 0x00010000,
		SPELL_ATTR1_CU_PANDEMIC_TIMER = 0x00020000,
		SPELL_ATTR1_CU_MISSILE_SPEED_IS_DELAY_IN_SEC = 0x00040000,
		SPELL_ATTR1_CU_ALLOW_DEFENSE_WHILE_CASTING = 0x00080000,
		SPELL_ATTR1_CU_DROP_STACK_ON_EXPIRE = 0x00100000,
		SPELL_ATTR1_CU_COMBODAMAGE = 0x00200000,
		SPELL_ATTR1_CU_COMBODURATION = 0x00400000,
		SPELL_ATTR1_CU_UNUSED55 = 0x00800000,
		SPELL_ATTR1_CU_UNUSED56 = 0x01000000,
		SPELL_ATTR1_CU_UNUSED57 = 0x02000000,
		SPELL_ATTR1_CU_UNUSED58 = 0x04000000,
		SPELL_ATTR1_CU_UNUSED59 = 0x08000000,
		SPELL_ATTR1_CU_UNUSED60 = 0x10000000,
		SPELL_ATTR1_CU_UNUSED61 = 0x20000000,
		SPELL_ATTR1_CU_UNUSED62 = 0x40000000,
		SPELL_ATTR1_CU_UNUSED63 = 0x80000000,
	};

	enum SpellAttr2Cu : uint32
	{
		SPELL_ATTR2_CU_TREAT_AS_INSTANT = 0x00000001,
		SPELL_ATTR2_CU_FORCE_HIDE_CASTBAR = 0x00000002,
		SPELL_ATTR2_CU_DO_NOT_DISPLAY_POWER_COST = 0x00000004,
		SPELL_ATTR2_CU_SUPPRESS_LEARN_MSG = 0x00000008,
		SPELL_ATTR2_CU_SUPPRESS_UNLEARN_MSG = 0x00000010,
		SPELL_ATTR2_CU_INVERT_CASTBAR = 0x00000020,
		SPELL_ATTR2_CU_LOW_TIME_TREAT_AS_INSTANT = 0x00000040,
		SPELL_ATTR2_CU_LOW_TIME_FORCE_HIDE_CASTBAR = 0x00000080,
		SPELL_ATTR2_CU_LOW_CAST_TIME_DONT_INTERRUPT = 0x00000100,
	};

	inline bool HasAttribute(SpellRow* spell, SpellAttr0 attribute)
	{
		return !!(spell->m_attributes & attribute);
	}
	inline bool HasAttribute(SpellRow* spell, SpellAttr1 attribute)
	{
		return !!(spell->m_attributesEx & attribute);
	}
	inline bool HasAttribute(SpellRow* spell, SpellAttr2 attribute)
	{
		return !!(spell->m_attributesExB & attribute);
	}
	inline bool HasAttribute(SpellRow* spell, SpellAttr3 attribute)
	{
		return !!(spell->m_attributesExC & attribute);
	}

}

namespace ClientData::SpellBook
{
	CLIENT_FUNCTION(UpdateSpells, 0x0053CA70, __cdecl, void, (int, int, int))

	inline void Refresh()
	{
		UpdateSpells(0, 0, 0);
	}
}

namespace ClientData::SpellCooldowns
{
	// SPELLHISTORY, 0x30 bytes. Offsets from SpellHistory__AddHistory (0x00805230) and
	// SpellHistory__GetCooldown (0x00807980).
	struct Entry
	{
		Entry* m_prevLink;
		Entry* m_next;
		uint32 m_spellId;
		uint32 m_itemId;
		uint32 m_startTime;
		uint32 m_duration;
		uint32 m_categoryId;
		uint32 m_categoryStart;
		uint32 m_categoryDuration;
		uint8 m_onHold;
		uint8 m_pad[3];
		uint32 m_gcdCategory;
		uint32 m_gcdDuration;
	};

	struct History
	{
		uint32 m_unk00;
		uint32 m_unk04;
		Entry* m_head;
		uint32 m_unk0C;
		uint32 m_unk10;
		Entry* m_free;
	};

	// s_SpellHistoryPlayer, index 0 is the player and index 1 the pet.
	CLIENT_ADDRESS(History, s_spellHistory, 0x00D3F5AC)

	constexpr uint32 PlayerHistory = 0;
	constexpr uint32 PetHistory = 1;
	constexpr uint32 HistoryCount = 2;

	// The client stops walking on a null pointer or one with bit 0 set, which is how the tail
	// sentinel links back to the list.
	inline Entry* Valid(Entry* entry)
	{
		return (!entry || (reinterpret_cast<uintptr_t>(entry) & 1)) ? nullptr : entry;
	}

	inline Entry* First(uint32 historyIndex)
	{
		return Valid(s_spellHistory[historyIndex].m_head);
	}

	inline Entry* Next(Entry* entry)
	{
		return Valid(entry->m_next);
	}

	CLIENT_FUNCTION(UpdateSpellBookCooldowns, 0x0053BAC0, __cdecl, void, ())
	CLIENT_FUNCTION(UpdateSpellBookUsable, 0x0053CF10, __cdecl, void, ())
	CLIENT_FUNCTION(UpdateContainerCooldowns, 0x005D6F10, __cdecl, void, ())
	CLIENT_FUNCTION(UpdatePetCooldowns, 0x005D3090, __cdecl, void, ())

	inline void RefreshUI()
	{
		UpdateSpellBookCooldowns();
		UpdateSpellBookUsable();
		UpdateContainerCooldowns();
		UpdatePetCooldowns();
	}
}
