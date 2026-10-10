#pragma once

#include <Defines.h>

#include <cstddef>
#include <cstdint>

namespace ClientData
{
	struct ChrClassesRow
	{
		int32 m_ID;
		int32 m_damageBonusStat;
		int32 m_displayPower;
		const char* m_petNameToken;
		const char* m_name;
		const char* m_nameFemale;
		const char* m_nameMale;
		const char* m_filename;
		int32 m_spellClassSet;
		int32 m_flags;
		int32 m_cinematicSequenceID;
		int32 m_requiredExpansion;
	};

	struct gtCombatRatingsRow
	{
		uint32 ID;
		float data;
	};

	struct gtOCTClassCombatRatingScalarRow
	{
		uint32 ID;
		float data;
	};

	struct MapRow
	{
		uint32 m_ID;
		char* m_Directory;
		uint32 m_InstanceType;
		uint32 m_Flags;
		uint32 m_PVP;
		char* m_MapName_lang;
		uint32 m_areaTableID;
		char* m_MapDescription0_lang;
		char* m_MapDescription1_lang;
		uint32 m_LoadingScreenID;
		float m_minimapIconScale;
		uint32 m_corpseMapID;
		float m_corpseX;
		float m_corpseY;
		uint32 m_timeOfDayOverride;
		uint32 m_expansionID;
		uint32 m_raidOffset;
		uint32 m_maxPlayers;
		uint32 m_parentMapID;
	};

	struct PowerDisplayRow
	{
		uint32 m_ID;
		uint32 m_actualType;
		char* m_globalStringBaseTag;
		uint8 m_red;
		uint8 m_green;
		uint8 m_blue;
	};

	struct SkillLineAbilityRow
	{
		uint32 m_ID;
		uint32 m_skillLine;
		uint32 m_spell;
		uint32 m_raceMask;
		uint32 m_classMask;
		uint32 m_excludeRace;
		uint32 m_excludeClass;
		uint32 m_minSkillLineRank;
		uint32 m_supercededBySpell;
		uint32 m_acquireMethod;
		uint32 m_trivialSkillLineRankHigh;
		uint32 m_trivialSkillLineRankLow;
		uint32 m_characterPoints[2];
		uint32 m_numSkillUps;
	};

	struct SkillLineRow
	{
		uint32 m_ID;
		uint32 m_categoryID;
		uint32 m_skillCostsID;
		char* m_displayName_lang;
		char* m_description_lang;
		uint32 m_spellIconID;
		char* m_alternateVerb_lang;
		uint32 m_canLink;
	};

	struct CreatureDisplayInfoRow
	{
		uint32 m_ID;
		uint32 m_modelId;
		uint32 m_soundId;
		uint32 m_extendedDisplayInfoId;
		float m_creatureModelScale;
	};

	struct CreatureModelDataRow
	{
		uint32 m_ID;
		uint32 m_flags;
		const char* m_modelName;
		uint32 m_sizeClass;
		float m_modelScale;
	};

	static_assert(offsetof(CreatureDisplayInfoRow, m_creatureModelScale) == 0x10, "CreatureDisplayInfo scale must sit at 0x10");
	static_assert(offsetof(CreatureModelDataRow, m_modelScale) == 0x10, "CreatureModelData scale must sit at 0x10");

	struct TotemCategoryRow
	{
		int32 m_ID;
		char* m_name;
		int32 m_totemCategoryType;
		int32 m_totemCategoryMask;
	};

	struct ItemSubClassMaskRow
	{
		uint32 m_classID;
		uint32 m_mask;
		char* m_name;
	};

	struct ItemSubClassRow
	{
		int32 m_classID;
		int32 m_subClassID;
		int32 m_prerequisiteProficiency;
		int32 m_postrequisiteProficiency;
		int32 m_flags;
		int32 m_displayFlags;
		int32 m_weaponParrySeq;
		int32 m_weaponReadySeq;
		int32 m_weaponAttackSeq;
		int32 m_weaponSwingSize;
		const char* m_displayName;
		const char* m_verboseName;
		int32 m_generatedID;
	};

	struct FactionRec
	{
		int32 m_ID;
		int32 m_reputationIndex;
		int32 m_reputationRaceMask[4];
		int32 m_reputationClassMask[4];
		int32 m_reputationBase[4];
		int32 m_reputationFlags[4];
		int32 m_parentFactionID;
		float m_parentFactionMod[2];
		int32 m_parentFactionCap[2];
		const char* m_name;
		const char* m_description;
	};

	struct SpellShapeshiftFormRow
	{
		int32 m_ID;
		int32 m_bonusActionBar;
		const char* m_name;
		int32 m_flags;
		int32 m_creatureType;
		int32 m_attackIconID;
		int32 m_combatRoundTime;
		int32 m_creatureDisplayID[4];
		int32 m_presetSpellID[8];
	};

	struct SpellIconRow
	{
		uint32 m_ID;
		char* m_textureFilename;
	};

	struct SpellRuneCostRow
	{
		int32 m_ID;
		int32 m_blood;
		int32 m_unholy;
		int32 m_frost;
		int32 m_runicPower;
	};

	// LiquidType.dbc. materialId is the anchor: CMapChunk::CreateLiquids reads it at [eax+38h]
	// before handing it to the LiquidMaterial lookup, which pins the layout either side of it.
	struct LiquidTypeRow
	{
		uint32 id;
		char const* name;
		uint32 flags;
		uint32 soundBank;
		uint32 soundId;
		uint32 spellId;
		float maxDarkenDepth;
		float fogDarkenIntensity;
		float ambDarkenIntensity;
		float dirDarkenIntensity;
		uint32 lightId;
		float particleScale;
		uint32 particleMovement;
		uint32 particleTexSlots;
		uint32 materialId;
	};

	static_assert(offsetof(LiquidTypeRow, materialId) == 0x38, "LiquidType MaterialID must sit at 0x38");
}
