#pragma once

#include <Defines.h>
#include <Macros.h>
#include <ClientData/System.h>

#include <cstddef>
#include <type_traits>

namespace ClientData::DB
{
	CLIENT_FUNCTION(GetRecord, 0x0065C290, __thiscall, void*, (void* recordView, int32 id))
	CLIENT_FUNCTION(DecompressRow, 0x004CFBB0, __cdecl, uint8*, (const void* src, int32 size, void* out))
	CLIENT_ADDRESS(char, IsCompressed, 0x00C5DEA0)

	enum class Storage
	{
		Inline,
		Packed
	};

	template <typename Rec, uintptr_t Address, Storage Kind = Storage::Inline>
	struct Table
	{
		using Record = Rec;
		static constexpr bool kPacked = Kind == Storage::Packed;

		static WoWClientDB* Db()
		{
			return reinterpret_cast<WoWClientDB*>(Address);
		}
		static bool IsLoaded()
		{
			return Db()->isLoaded != 0;
		}
		static int32 Count()
		{
			return IsLoaded() ? Db()->numRows : 0;
		}
		static int32 MinId()
		{
			return Db()->minIndex;
		}
		static int32 MaxId()
		{
			return Db()->maxIndex;
		}

		static const Rec* Get(int32 id)
		{
			static_assert(!kPacked, "packed rows may be RLE compressed, read them with Copy");
			if (!IsLoaded())
				return nullptr;

			return static_cast<const Rec*>(GetRecord(&Db()->funcTable2, id));
		}

		static bool Copy(int32 id, Rec& out)
		{
			if (!IsLoaded())
				return false;

			return Unpack(GetRecord(&Db()->funcTable2, id), out);
		}

		static const Rec* At(int32 index)
		{
			static_assert(!kPacked, "packed rows may be RLE compressed, read them with CopyAt");
			if (index < 0 || index >= Count())
				return nullptr;

			return reinterpret_cast<const Rec*>(Db()->FirstRow) + index;
		}

		static bool CopyAt(int32 index, Rec& out)
		{
			if (index < 0 || index >= Count())
				return false;

			if constexpr (kPacked)
				return Unpack(reinterpret_cast<void* const*>(Db()->FirstRow)[index], out);
			else
				return Unpack(reinterpret_cast<const Rec*>(Db()->FirstRow) + index, out);
		}

		template <typename Fn>
		static void ForEach(Fn&& fn)
		{
			int32 count = Count();
			for (int32 i = 0; i < count; ++i)
			{
				Rec copy;
				const Rec* row = &copy;
				if constexpr (kPacked)
				{
					if (!CopyAt(i, copy))
						continue;
				}
				else
					row = At(i);

				if constexpr (std::is_same_v<std::invoke_result_t<Fn&, const Rec&>, bool>)
				{
					if (!fn(*row))
						return;
				}
				else
					fn(*row);
			}
		}

		template <typename Pred>
		static bool Find(Pred&& pred, Rec& out)
		{
			bool found = false;
			ForEach([&](const Rec& row)
			{
				if (!pred(row))
					return true;

				out = row;
				found = true;
				return false;
			});
			return found;
		}

	private:
		static bool Unpack(const void* raw, Rec& out)
		{
			if (!raw)
				return false;

			if (kPacked && *IsCompressed)
				DecompressRow(raw, static_cast<int32>(sizeof(Rec)), &out);
			else
				out = *static_cast<const Rec*>(raw);
			return true;
		}
	};

	struct Achievement_CategoryRec
	{
		uint32 m_ID;
		int32 m_parent;
		const char* m_name;
		uint32 m_uiOrder;
	};
	static_assert(sizeof(Achievement_CategoryRec) == 0x10);

	struct Achievement_CriteriaRec
	{
		uint32 m_ID;
		uint32 m_achievementID;
		uint32 m_type;
		uint32 m_assetID;
		uint32 m_quantity;
		uint32 m_startEvent;
		uint32 m_startAsset;
		uint32 m_failEvent;
		uint32 m_failAsset;
		const char* m_description;
		uint32 m_flags;
		uint32 m_timerStartEvent;
		uint32 m_timerAssetID;
		uint32 m_timerTime;
		uint32 m_uiOrder;
	};
	static_assert(sizeof(Achievement_CriteriaRec) == 0x3C);

	struct AchievementRec
	{
		uint32 m_ID;
		int32 m_faction;
		int32 m_instanceID;
		uint32 m_supercedes;
		const char* m_title;
		const char* m_description;
		uint32 m_category;
		uint32 m_points;
		uint32 m_uiOrder;
		uint32 m_flags;
		uint32 m_iconID;
		const char* m_reward;
		uint32 m_minimumCriteria;
		uint32 m_sharesCriteria;
	};
	static_assert(sizeof(AchievementRec) == 0x38);

	struct AnimationDataRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_weaponflags;
		uint32 m_bodyflags;
		uint32 m_flags;
		uint32 m_fallback;
		uint32 m_behaviorID;
		uint32 m_behaviorTier;
	};
	static_assert(sizeof(AnimationDataRec) == 0x20);

	struct AreaGroupRec
	{
		uint32 m_ID;
		uint32 m_areaID[6];
		uint32 m_nextAreaID;
	};
	static_assert(sizeof(AreaGroupRec) == 0x20);

	struct AreaPOIRec
	{
		uint32 m_ID;
		uint32 m_importance;
		uint32 m_icon[9];
		uint32 m_factionID;
		float m_pos[3];
		uint32 m_continentID;
		uint32 m_flags;
		int32 m_areaID;
		const char* m_name;
		const char* m_description;
		uint32 m_worldStateID;
		uint32 m_worldMapLink;
	};
	static_assert(sizeof(AreaPOIRec) == 0x58);

	struct AreaTableRec
	{
		uint32 m_ID;
		uint32 m_continentID;
		uint32 m_parentAreaID;
		uint32 m_areaBit;
		uint32 m_flags;
		uint32 m_soundProviderPref;
		uint32 m_soundProviderPrefUnderwater;
		uint32 m_ambienceID;
		uint32 m_zoneMusic;
		uint32 m_introSound;
		int32 m_explorationLevel;
		const char* m_areaName;
		uint32 m_factionGroupMask;
		uint32 m_liquidTypeID[4];
		float m_minElevation;
		float m_ambientMultiplier;
		uint32 m_lightID;
	};
	static_assert(sizeof(AreaTableRec) == 0x50);

	struct AreaTriggerRec
	{
		uint32 m_ID;
		uint32 m_continentID;
		float m_pos[3];
		float m_radius;
		float m_boxLength;
		float m_boxWidth;
		float m_boxHeight;
		float m_boxYaw;
	};
	static_assert(sizeof(AreaTriggerRec) == 0x28);

	struct AttackAnimKitsRec
	{
		uint32 m_ID;
		uint32 m_itemSubclassID;
		uint32 m_animTypeID;
		uint32 m_animFrequency;
		uint32 m_whichHand;
	};
	static_assert(sizeof(AttackAnimKitsRec) == 0x14);

	struct AttackAnimTypesRec
	{
		uint32 m_animID;
		const char* m_animName;
	};
	static_assert(sizeof(AttackAnimTypesRec) == 0x8);

	struct AuctionHouseRec
	{
		uint32 m_ID;
		uint32 m_factionID;
		uint32 m_depositRate;
		uint32 m_consignmentRate;
		const char* m_name;
	};
	static_assert(sizeof(AuctionHouseRec) == 0x14);

	struct BankBagSlotPricesRec
	{
		uint32 m_ID;
		uint32 m_cost;
	};
	static_assert(sizeof(BankBagSlotPricesRec) == 0x8);

	struct BannedAddonsRec
	{
		uint32 m_ID;
		uint32 m_nameMd5[4];
		uint32 m_versionMd5[4];
		uint32 m_lastModified;
		uint32 m_flags;
	};
	static_assert(sizeof(BannedAddonsRec) == 0x2C);

	struct BarberShopStyleRec
	{
		uint32 m_ID;
		uint32 m_type;
		const char* m_displayName;
		const char* m_description;
		float m_costModifier;
		uint32 m_race;
		uint32 m_sex;
		uint32 m_data;
	};
	static_assert(sizeof(BarberShopStyleRec) == 0x20);

	struct BattlemasterListRec
	{
		uint32 m_ID;
		int32 m_mapID[8];
		uint32 m_instanceType;
		uint32 m_groupsAllowed;
		const char* m_name;
		uint32 m_maxGroupSize;
		uint32 m_holidayWorldState;
		uint32 m_minLevel;
		uint32 m_maxLevel;
	};
	static_assert(sizeof(BattlemasterListRec) == 0x40);

	struct CameraShakesRec
	{
		uint32 m_ID;
		uint32 m_shakeType;
		uint32 m_direction;
		float m_amplitude;
		float m_frequency;
		float m_duration;
		float m_phase;
		float m_coefficient;
	};
	static_assert(sizeof(CameraShakesRec) == 0x20);

	struct Cfg_CategoriesRec
	{
		uint32 m_ID;
		uint32 m_localeMask;
		uint32 m_createCharsetMask;
		uint32 m_flags;
		const char* m_name;
	};
	static_assert(sizeof(Cfg_CategoriesRec) == 0x14);

	struct Cfg_ConfigsRec
	{
		uint32 m_ID;
		uint32 m_realmType;
		uint32 m_playerKillingAllowed;
		uint32 m_roleplaying;
		int32 m_unread_10;
	};
	static_assert(sizeof(Cfg_ConfigsRec) == 0x14);

	struct CharacterFacialHairStylesRec
	{
		uint32 m_raceID;
		uint32 m_sexID;
		uint32 m_variationID;
		uint32 m_geoset[5];
		int32 m_generatedID;
	};
	static_assert(sizeof(CharacterFacialHairStylesRec) == 0x24);

	struct CharBaseInfoRec
	{
		uint8 m_raceID;
		uint8 m_classID;
		uint8 m_pad2[2];
		int32 m_generatedID;
	};
	static_assert(sizeof(CharBaseInfoRec) == 0x8);

	struct CharHairGeosetsRec
	{
		uint32 m_ID;
		uint32 m_raceID;
		uint32 m_sexID;
		uint32 m_variationID;
		uint32 m_geosetID;
		uint32 m_showscalp;
	};
	static_assert(sizeof(CharHairGeosetsRec) == 0x18);

	struct CharSectionsRec
	{
		uint32 m_ID;
		uint32 m_raceID;
		uint32 m_sexID;
		uint32 m_baseSection;
		const char* m_textureName[3];
		uint32 m_flags;
		uint32 m_variationIndex;
		uint32 m_colorIndex;
	};
	static_assert(sizeof(CharSectionsRec) == 0x28);

	struct CharStartOutfitRec
	{
		uint32 m_ID;
		uint8 m_raceID;
		uint8 m_classID;
		uint8 m_sexID;
		uint8 m_outfitID;
		int32 m_itemID[24];
		int32 m_displayItemID[24];
		int32 m_inventoryType[24];
	};
	static_assert(sizeof(CharStartOutfitRec) == 0x128);

	struct CharTitlesRec
	{
		uint32 m_ID;
		uint32 m_conditionID;
		const char* m_name;
		const char* m_name1;
		uint32 m_maskID;
	};
	static_assert(sizeof(CharTitlesRec) == 0x14);

	struct ChatChannelsRec
	{
		uint32 m_ID;
		uint32 m_flags;
		uint32 m_factionGroup;
		const char* m_name;
		const char* m_shortcut;
	};
	static_assert(sizeof(ChatChannelsRec) == 0x14);

	struct ChatProfanityRec
	{
		uint32 m_ID;
		const char* m_text;
		int32 m_language;
	};
	static_assert(sizeof(ChatProfanityRec) == 0xC);

	struct ChrClassesRec
	{
		uint32 m_ID;
		uint32 m_damageBonusStat;
		uint32 m_displayPower;
		const char* m_petNameToken;
		const char* m_name;
		const char* m_nameFemale;
		const char* m_nameMale;
		const char* m_filename;
		uint32 m_spellClassSet;
		uint32 m_flags;
		uint32 m_cinematicSequenceID;
		uint32 m_requiredExpansion;
	};
	static_assert(sizeof(ChrClassesRec) == 0x30);

	struct ChrRacesRec
	{
		uint32 m_ID;
		uint32 m_flags;
		uint32 m_factionID;
		uint32 m_explorationSoundID;
		uint32 m_maleDisplayID;
		uint32 m_femaleDisplayID;
		const char* m_clientPrefix;
		uint32 m_baseLanguage;
		uint32 m_creatureType;
		uint32 m_resSicknessSpellID;
		uint32 m_splashSoundID;
		const char* m_clientFileString;
		uint32 m_cinematicSequenceID;
		uint32 m_alliance;
		const char* m_name;
		const char* m_nameFemale;
		const char* m_nameMale;
		const char* m_facialHairCustomization[2];
		const char* m_hairCustomization;
		uint32 m_requiredExpansion;
	};
	static_assert(sizeof(ChrRacesRec) == 0x54);

	struct CinematicCameraRec
	{
		uint32 m_ID;
		const char* m_model;
		uint32 m_soundID;
		float m_origin[3];
		float m_originFacing;
	};
	static_assert(sizeof(CinematicCameraRec) == 0x1C);

	struct CinematicSequencesRec
	{
		uint32 m_ID;
		uint32 m_soundID;
		uint32 m_camera[8];
	};
	static_assert(sizeof(CinematicSequencesRec) == 0x28);

	struct CreatureDisplayInfoExtraRec
	{
		uint32 m_ID;
		uint32 m_displayRaceID;
		uint32 m_displaySexID;
		uint32 m_skinID;
		uint32 m_faceID;
		uint32 m_hairStyleID;
		uint32 m_hairColorID;
		uint32 m_facialHairID;
		uint32 m_npcitemDisplay[11];
		uint32 m_flags;
		const char* m_bakeName;
	};
	static_assert(sizeof(CreatureDisplayInfoExtraRec) == 0x54);

	struct CreatureDisplayInfoRec
	{
		uint32 m_ID;
		uint32 m_modelID;
		uint32 m_soundID;
		uint32 m_extendedDisplayInfoID;
		float m_creatureModelScale;
		uint32 m_creatureModelAlpha;
		const char* m_textureVariation[3];
		const char* m_portraitTextureName;
		int32 m_sizeClass;
		uint32 m_bloodID;
		uint32 m_npcsoundID;
		uint32 m_particleColorID;
		uint32 m_creatureGeosetData;
		uint32 m_objectEffectPackageID;
	};
	static_assert(sizeof(CreatureDisplayInfoRec) == 0x40);

	struct CreatureFamilyRec
	{
		uint32 m_ID;
		float m_minScale;
		uint32 m_minScaleLevel;
		float m_maxScale;
		uint32 m_maxScaleLevel;
		uint32 m_skillLine[2];
		uint32 m_petFoodMask;
		int32 m_petTalentType;
		int32 m_categoryEnumID;
		const char* m_name;
		const char* m_iconFile;
	};
	static_assert(sizeof(CreatureFamilyRec) == 0x30);

	struct CreatureModelDataRec
	{
		uint32 m_ID;
		uint32 m_flags;
		const char* m_modelName;
		uint32 m_sizeClass;
		float m_modelScale;
		int32 m_bloodID;
		int32 m_footprintTextureID;
		float m_footprintTextureLength;
		float m_footprintTextureWidth;
		float m_footprintParticleScale;
		uint32 m_foleyMaterialID;
		uint32 m_footstepShakeSize;
		uint32 m_deathThudShakeSize;
		uint32 m_soundID;
		float m_collisionWidth;
		float m_collisionHeight;
		float m_mountHeight;
		float m_geoBoxMinX;
		float m_geoBoxMinY;
		float m_geoBoxMinZ;
		float m_geoBoxMaxX;
		float m_geoBoxMaxY;
		float m_geoBoxMaxZ;
		float m_worldEffectScale;
		float m_attachedEffectScale;
		float m_missileCollisionRadius;
		float m_missileCollisionPush;
		float m_missileCollisionRaise;
	};
	static_assert(sizeof(CreatureModelDataRec) == 0x70);

	struct CreatureMovementInfoRec
	{
		uint32 m_ID;
		float m_smoothFacingChaseRate;
	};
	static_assert(sizeof(CreatureMovementInfoRec) == 0x8);

	struct CreatureSoundDataRec
	{
		uint32 m_ID;
		uint32 m_soundExertionID;
		uint32 m_soundExertionCriticalID;
		uint32 m_soundInjuryID;
		uint32 m_soundInjuryCriticalID;
		uint32 m_soundInjuryCrushingBlowID;
		uint32 m_soundDeathID;
		uint32 m_soundStunID;
		uint32 m_soundStandID;
		uint32 m_soundFootstepID;
		uint32 m_soundAggroID;
		uint32 m_soundWingFlapID;
		uint32 m_soundWingGlideID;
		uint32 m_soundAlertID;
		uint32 m_soundFidget[5];
		uint32 m_customAttack[4];
		uint32 m_npcsoundID;
		uint32 m_loopSoundID;
		uint32 m_creatureImpactType;
		uint32 m_soundJumpStartID;
		uint32 m_soundJumpEndID;
		uint32 m_soundPetAttackID;
		uint32 m_soundPetOrderID;
		uint32 m_soundPetDismissID;
		float m_fidgetDelaySecondsMin;
		float m_fidgetDelaySecondsMax;
		uint32 m_birthSoundID;
		uint32 m_spellCastDirectedSoundID;
		uint32 m_submergeSoundID;
		uint32 m_submergedSoundID;
		uint32 m_creatureSoundDataIdpet;
	};
	static_assert(sizeof(CreatureSoundDataRec) == 0x98);

	struct CreatureSpellDataRec
	{
		uint32 m_ID;
		uint32 m_spells[4];
		uint32 m_availability[4];
	};
	static_assert(sizeof(CreatureSpellDataRec) == 0x24);

	struct CreatureTypeRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_flags;
	};
	static_assert(sizeof(CreatureTypeRec) == 0xC);

	struct CurrencyCategoryRec
	{
		uint32 m_ID;
		uint32 m_flags;
		const char* m_name;
	};
	static_assert(sizeof(CurrencyCategoryRec) == 0xC);

	struct CurrencyTypesRec
	{
		uint32 m_ID;
		uint32 m_itemID;
		uint32 m_categoryID;
		uint32 m_bitIndex;
	};
	static_assert(sizeof(CurrencyTypesRec) == 0x10);

	struct DanceMovesRec
	{
		uint32 m_ID;
		uint32 m_type;
		uint32 m_param;
		uint32 m_fallback;
		uint32 m_racemask;
		const char* m_internalName;
		const char* m_name;
		uint32 m_lockID;
	};
	static_assert(sizeof(DanceMovesRec) == 0x20);

	struct DeathThudLookupsRec
	{
		uint32 m_ID;
		uint32 m_sizeClass;
		uint32 m_terrainTypeSoundID;
		uint32 m_soundEntryID;
		uint32 m_soundEntryIdwater;
	};
	static_assert(sizeof(DeathThudLookupsRec) == 0x14);

	struct DestructibleModelDataRec
	{
		uint32 m_ID;
		uint32 m_state0ImpactEffectDoodadSet;
		uint32 m_state0AmbientDoodadSet;
		uint32 m_state1WMO;
		uint32 m_state1DestructionDoodadSet;
		uint32 m_state1ImpactEffectDoodadSet;
		uint32 m_state1AmbientDoodadSet;
		uint32 m_state2WMO;
		uint32 m_state2DestructionDoodadSet;
		uint32 m_state2ImpactEffectDoodadSet;
		uint32 m_state2AmbientDoodadSet;
		uint32 m_state3WMO;
		uint32 m_state3InitDoodadSet;
		uint32 m_state3AmbientDoodadSet;
		uint32 m_ejectDirection;
		uint32 m_repairGroundFx;
		uint32 m_doNotHighlight;
		uint32 m_healEffect;
		uint32 m_healEffectSpeed;
	};
	static_assert(sizeof(DestructibleModelDataRec) == 0x4C);

	struct DungeonEncounterRec
	{
		uint32 m_ID;
		uint32 m_mapID;
		uint32 m_difficulty;
		int32 m_orderIndex;
		uint32 m_bit;
		const char* m_name;
		uint32 m_spellIconID;
	};
	static_assert(sizeof(DungeonEncounterRec) == 0x1C);

	struct DungeonMapChunkRec
	{
		uint32 m_ID;
		uint32 m_mapID;
		uint32 m_wmogroupID;
		uint32 m_dungeonMapID;
		float m_minZ;
	};
	static_assert(sizeof(DungeonMapChunkRec) == 0x14);

	struct DungeonMapRec
	{
		uint32 m_ID;
		uint32 m_mapID;
		uint32 m_floorIndex;
		float m_minX;
		float m_maxX;
		float m_minY;
		float m_maxY;
		uint32 m_parentWorldMapID;
	};
	static_assert(sizeof(DungeonMapRec) == 0x20);

	struct DurabilityCostsRec
	{
		uint32 m_ID;
		uint32 m_weaponSubClassCost[21];
		uint32 m_armorSubClassCost[8];
	};
	static_assert(sizeof(DurabilityCostsRec) == 0x78);

	struct DurabilityQualityRec
	{
		uint32 m_ID;
		float m_data;
	};
	static_assert(sizeof(DurabilityQualityRec) == 0x8);

	struct EmotesRec
	{
		uint32 m_ID;
		const char* m_emoteSlashCommand;
		uint32 m_animID;
		uint32 m_emoteFlags;
		uint32 m_emoteSpecProc;
		uint32 m_emoteSpecProcParam;
		int32 m_eventSoundID;
	};
	static_assert(sizeof(EmotesRec) == 0x1C);

	struct EmotesTextDataRec
	{
		uint32 m_ID;
		const char* m_text;
	};
	static_assert(sizeof(EmotesTextDataRec) == 0x8);

	struct EmotesTextRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_emoteID;
		uint32 m_emoteText[16];
	};
	static_assert(sizeof(EmotesTextRec) == 0x4C);

	struct EmotesTextSoundRec
	{
		uint32 m_ID;
		uint32 m_emotesTextID;
		uint32 m_raceID;
		uint32 m_sexID;
		uint32 m_soundID;
	};
	static_assert(sizeof(EmotesTextSoundRec) == 0x14);

	struct EnvironmentalDamageRec
	{
		uint32 m_ID;
		uint32 m_enumID;
		uint32 m_visualkitID;
	};
	static_assert(sizeof(EnvironmentalDamageRec) == 0xC);

	struct ExhaustionRec
	{
		uint32 m_ID;
		int32 m_xp;
		float m_factor;
		float m_outdoorHours;
		float m_innHours;
		const char* m_name;
		float m_threshold;
	};
	static_assert(sizeof(ExhaustionRec) == 0x1C);

	struct FactionGroupRec
	{
		uint32 m_ID;
		uint32 m_maskID;
		const char* m_internalName;
		const char* m_name;
	};
	static_assert(sizeof(FactionGroupRec) == 0x10);

	struct FactionRec
	{
		uint32 m_ID;
		int32 m_reputationIndex;
		uint32 m_reputationRaceMask[4];
		uint32 m_reputationClassMask[4];
		int32 m_reputationBase[4];
		uint32 m_reputationFlags[4];
		uint32 m_parentFactionID;
		float m_parentFactionMod[2];
		uint32 m_parentFactionCap[2];
		const char* m_name;
		const char* m_description;
	};
	static_assert(sizeof(FactionRec) == 0x64);

	struct FactionTemplateRec
	{
		uint32 m_ID;
		uint32 m_faction;
		uint32 m_flags;
		uint32 m_factionGroup;
		uint32 m_friendGroup;
		uint32 m_enemyGroup;
		uint32 m_enemies[4];
		uint32 m_friend[4];
	};
	static_assert(sizeof(FactionTemplateRec) == 0x38);

	struct FileDataRec
	{
		uint32 m_ID;
		const char* m_filename;
		const char* m_filepath;
	};
	static_assert(sizeof(FileDataRec) == 0xC);

	struct FootprintTexturesRec
	{
		uint32 m_ID;
		const char* m_footstepFilename;
	};
	static_assert(sizeof(FootprintTexturesRec) == 0x8);

	struct FootstepTerrainLookupRec
	{
		uint32 m_ID;
		uint32 m_creatureFootstepID;
		int32 m_terrainSoundID;
		uint32 m_soundID;
		uint32 m_soundIdsplash;
	};
	static_assert(sizeof(FootstepTerrainLookupRec) == 0x14);

	struct GameObjectArtKitRec
	{
		uint32 m_ID;
		const char* m_textureVariation[3];
		const char* m_attachModel[4];
	};
	static_assert(sizeof(GameObjectArtKitRec) == 0x20);

	struct GameObjectDisplayInfoRec
	{
		uint32 m_ID;
		const char* m_modelName;
		uint32 m_sound[10];
		float m_geoBoxMin[3];
		float m_geoBoxMax[3];
		uint32 m_objectEffectPackageID;
	};
	static_assert(sizeof(GameObjectDisplayInfoRec) == 0x4C);

	struct GameTablesRec
	{
		const char* m_name;
		uint32 m_numRows;
		uint32 m_numColumns;
		int32 m_generatedID;
	};
	static_assert(sizeof(GameTablesRec) == 0x10);

	struct GameTipsRec
	{
		uint32 m_ID;
		const char* m_text;
	};
	static_assert(sizeof(GameTipsRec) == 0x8);

	struct GemPropertiesRec
	{
		uint32 m_ID;
		uint32 m_enchantID;
		uint32 m_maxcountInv;
		uint32 m_maxcountItem;
		uint32 m_type;
	};
	static_assert(sizeof(GemPropertiesRec) == 0x14);

	struct GlyphPropertiesRec
	{
		uint32 m_ID;
		uint32 m_spellID;
		uint32 m_glyphSlotFlags;
		uint32 m_spellIconID;
	};
	static_assert(sizeof(GlyphPropertiesRec) == 0x10);

	struct GlyphSlotRec
	{
		uint32 m_ID;
		uint32 m_type;
		uint32 m_tooltip;
	};
	static_assert(sizeof(GlyphSlotRec) == 0xC);

	struct GMSurveyAnswersRec
	{
		uint32 m_ID;
		uint32 m_sortIndex;
		uint32 m_gmsurveyQuestionID;
		const char* m_answer;
	};
	static_assert(sizeof(GMSurveyAnswersRec) == 0x10);

	struct GMSurveyCurrentSurveyRec
	{
		uint32 m_langid;
		uint32 m_gmsurveyID;
	};
	static_assert(sizeof(GMSurveyCurrentSurveyRec) == 0x8);

	struct GMSurveyQuestionsRec
	{
		uint32 m_ID;
		const char* m_question;
	};
	static_assert(sizeof(GMSurveyQuestionsRec) == 0x8);

	struct GMSurveySurveysRec
	{
		uint32 m_ID;
		uint32 m_[10];
	};
	static_assert(sizeof(GMSurveySurveysRec) == 0x2C);

	struct GMTicketCategoryRec
	{
		uint32 m_ID;
		const char* m_category;
	};
	static_assert(sizeof(GMTicketCategoryRec) == 0x8);

	struct GroundEffectDoodadRec
	{
		uint32 m_ID;
		const char* m_doodadpath;
		uint32 m_flags;
	};
	static_assert(sizeof(GroundEffectDoodadRec) == 0xC);

	struct GroundEffectTextureRec
	{
		uint32 m_ID;
		uint32 m_doodadID[4];
		uint32 m_doodadWeight[4];
		uint32 m_density;
		uint32 m_sound;
	};
	static_assert(sizeof(GroundEffectTextureRec) == 0x2C);

	struct gtBarberShopCostBaseRec
	{
		float m_data;
		int32 m_generatedID;
	};
	static_assert(sizeof(gtBarberShopCostBaseRec) == 0x8);

	struct gtChanceToMeleeCritBaseRec
	{
		float m_data;
		int32 m_generatedID;
	};
	static_assert(sizeof(gtChanceToMeleeCritBaseRec) == 0x8);

	struct gtChanceToMeleeCritRec
	{
		float m_data;
		int32 m_generatedID;
	};
	static_assert(sizeof(gtChanceToMeleeCritRec) == 0x8);

	struct gtChanceToSpellCritBaseRec
	{
		float m_data;
		int32 m_generatedID;
	};
	static_assert(sizeof(gtChanceToSpellCritBaseRec) == 0x8);

	struct gtChanceToSpellCritRec
	{
		float m_data;
		int32 m_generatedID;
	};
	static_assert(sizeof(gtChanceToSpellCritRec) == 0x8);

	struct gtCombatRatingsRec
	{
		float m_data;
		int32 m_generatedID;
	};
	static_assert(sizeof(gtCombatRatingsRec) == 0x8);

	struct gtNPCManaCostScalerRec
	{
		float m_data;
		int32 m_generatedID;
	};
	static_assert(sizeof(gtNPCManaCostScalerRec) == 0x8);

	struct gtOCTClassCombatRatingScalarRec
	{
		uint32 m_ID;
		float m_data;
	};
	static_assert(sizeof(gtOCTClassCombatRatingScalarRec) == 0x8);

	struct gtOCTRegenHPRec
	{
		float m_data;
		int32 m_generatedID;
	};
	static_assert(sizeof(gtOCTRegenHPRec) == 0x8);

	struct gtOCTRegenMPRec
	{
		float m_data;
		int32 m_generatedID;
	};
	static_assert(sizeof(gtOCTRegenMPRec) == 0x8);

	struct gtRegenHPPerSptRec
	{
		float m_data;
		int32 m_generatedID;
	};
	static_assert(sizeof(gtRegenHPPerSptRec) == 0x8);

	struct gtRegenMPPerSptRec
	{
		float m_data;
		int32 m_generatedID;
	};
	static_assert(sizeof(gtRegenMPPerSptRec) == 0x8);

	struct HelmetGeosetVisDataRec
	{
		uint32 m_ID;
		int32 m_hideGeoset[7];
	};
	static_assert(sizeof(HelmetGeosetVisDataRec) == 0x20);

	struct HolidayDescriptionsRec
	{
		uint32 m_ID;
		const char* m_description;
	};
	static_assert(sizeof(HolidayDescriptionsRec) == 0x8);

	struct HolidayNamesRec
	{
		uint32 m_ID;
		const char* m_name;
	};
	static_assert(sizeof(HolidayNamesRec) == 0x8);

	struct HolidaysRec
	{
		uint32 m_ID;
		uint32 m_duration[10];
		uint32 m_date[26];
		uint32 m_region;
		uint32 m_looping;
		uint32 m_calendarFlags[10];
		uint32 m_holidayNameID;
		uint32 m_holidayDescriptionID;
		const char* m_textureFileName;
		uint32 m_priority;
		int32 m_calendarFilterType;
		uint32 m_flags;
	};
	static_assert(sizeof(HolidaysRec) == 0xDC);

	struct ItemBagFamilyRec
	{
		uint32 m_ID;
		const char* m_name;
	};
	static_assert(sizeof(ItemBagFamilyRec) == 0x8);

	struct ItemClassRec
	{
		uint32 m_classID;
		uint32 m_subclassMapID;
		uint32 m_flags;
		const char* m_className;
		int32 m_generatedID;
	};
	static_assert(sizeof(ItemClassRec) == 0x14);

	struct ItemCondExtCostsRec
	{
		uint32 m_ID;
		uint32 m_condExtendedCost;
		uint32 m_itemExtendedCostEntry;
		uint32 m_arenaSeason;
	};
	static_assert(sizeof(ItemCondExtCostsRec) == 0x10);

	struct ItemDisplayInfoRec
	{
		uint32 m_ID;
		const char* m_modelName[2];
		const char* m_modelTexture[2];
		const char* m_inventoryIcon[2];
		uint32 m_geosetGroup[3];
		uint32 m_flags;
		uint32 m_spellVisualID;
		uint32 m_groupSoundIndex;
		uint32 m_helmetGeosetVisID[2];
		const char* m_texture[8];
		int32 m_itemVisual;
		uint32 m_particleColorID;
	};
	static_assert(sizeof(ItemDisplayInfoRec) == 0x64);

	struct ItemExtendedCostRec
	{
		uint32 m_ID;
		uint32 m_honorPoints;
		uint32 m_arenaPoints;
		uint32 m_arenaBracket;
		uint32 m_itemID[5];
		uint32 m_itemCount[5];
		uint32 m_requiredArenaRating;
		uint32 m_itemPurchaseGroup;
	};
	static_assert(sizeof(ItemExtendedCostRec) == 0x40);

	struct ItemGroupSoundsRec
	{
		uint32 m_ID;
		uint32 m_sound[4];
	};
	static_assert(sizeof(ItemGroupSoundsRec) == 0x14);

	struct ItemLimitCategoryRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_quantity;
		uint32 m_flags;
	};
	static_assert(sizeof(ItemLimitCategoryRec) == 0x10);

	struct ItemPetFoodRec
	{
		uint32 m_ID;
		const char* m_name;
	};
	static_assert(sizeof(ItemPetFoodRec) == 0x8);

	struct ItemPurchaseGroupRec
	{
		uint32 m_ID;
		uint32 m_itemID[8];
		const char* m_name;
	};
	static_assert(sizeof(ItemPurchaseGroupRec) == 0x28);

	struct ItemRandomPropertiesRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_enchantment[5];
		const char* m_name_lang;
	};
	static_assert(sizeof(ItemRandomPropertiesRec) == 0x20);

	struct ItemRandomSuffixRec
	{
		uint32 m_ID;
		const char* m_name;
		const char* m_internalName;
		uint32 m_enchantment[5];
		uint32 m_allocationPct[5];
	};
	static_assert(sizeof(ItemRandomSuffixRec) == 0x34);

	struct ItemRec
	{
		uint32 m_ID;
		uint32 m_classID;
		uint32 m_subclassID;
		int32 m_soundOverrideSubclassID;
		int32 m_material;
		uint32 m_displayInfoID;
		uint32 m_inventoryType;
		uint32 m_sheatheType;
	};
	static_assert(sizeof(ItemRec) == 0x20);

	struct ItemSetRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_itemID[17];
		uint32 m_setSpellID[8];
		uint32 m_setThreshold[8];
		uint32 m_requiredSkill;
		uint32 m_requiredSkillRank;
	};
	static_assert(sizeof(ItemSetRec) == 0x94);

	struct ItemSubClassMaskRec
	{
		uint32 m_classID;
		uint32 m_mask;
		const char* m_name;
		int32 m_generatedID;
	};
	static_assert(sizeof(ItemSubClassMaskRec) == 0x10);

	struct ItemSubClassRec
	{
		uint32 m_classID;
		uint32 m_subClassID;
		int32 m_prerequisiteProficiency;
		int32 m_postrequisiteProficiency;
		uint32 m_flags;
		uint32 m_displayFlags;
		uint32 m_weaponParrySeq;
		uint32 m_weaponReadySeq;
		uint32 m_weaponAttackSeq;
		uint32 m_weaponSwingSize;
		const char* m_displayName;
		const char* m_verboseName;
		int32 m_generatedID;
	};
	static_assert(sizeof(ItemSubClassRec) == 0x34);

	struct ItemVisualEffectsRec
	{
		uint32 m_ID;
		const char* m_model;
	};
	static_assert(sizeof(ItemVisualEffectsRec) == 0x8);

	struct ItemVisualsRec
	{
		uint32 m_ID;
		int32 m_slot[5];
	};
	static_assert(sizeof(ItemVisualsRec) == 0x18);

	struct LanguagesRec
	{
		uint32 m_ID;
		const char* m_name;
	};
	static_assert(sizeof(LanguagesRec) == 0x8);

	struct LanguageWordsRec
	{
		uint32 m_ID;
		uint32 m_languageID;
		const char* m_word;
	};
	static_assert(sizeof(LanguageWordsRec) == 0xC);

	struct LFGDungeonExpansionRec
	{
		uint32 m_ID;
		uint32 m_lfgID;
		uint32 m_expansionLevel;
		uint32 m_randomID;
		uint32 m_hardLevelMin;
		uint32 m_hardLevelMax;
		uint32 m_targetLevelMin;
		uint32 m_targetLevelMax;
	};
	static_assert(sizeof(LFGDungeonExpansionRec) == 0x20);

	struct LFGDungeonGroupRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_orderIndex;
		uint32 m_parentGroupID;
		uint32 m_typeID;
	};
	static_assert(sizeof(LFGDungeonGroupRec) == 0x14);

	struct LFGDungeonsRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_minLevel;
		uint32 m_maxLevel;
		uint32 m_targetLevel;
		uint32 m_targetLevelMin;
		uint32 m_targetLevelMax;
		int32 m_mapID;
		uint32 m_difficulty;
		uint32 m_flags;
		uint32 m_typeID;
		int32 m_faction;
		const char* m_textureFilename;
		uint32 m_expansionLevel;
		uint32 m_orderIndex;
		uint32 m_groupID;
		const char* m_description;
	};
	static_assert(sizeof(LFGDungeonsRec) == 0x44);

	struct LightFloatBandRec
	{
		uint32 m_ID;
		uint32 m_num;
		uint32 m_time[16];
		float m_data[16];
	};
	static_assert(sizeof(LightFloatBandRec) == 0x88);

	struct LightIntBandRec
	{
		uint32 m_ID;
		uint32 m_num;
		uint32 m_time[16];
		uint32 m_data[16];
	};
	static_assert(sizeof(LightIntBandRec) == 0x88);

	struct LightParamsRec
	{
		uint32 m_ID;
		uint32 m_highlightSky;
		uint32 m_lightSkyboxID;
		float m_glow;
		float m_waterShallowAlpha;
		float m_waterDeepAlpha;
		float m_oceanShallowAlpha;
		float m_oceanDeepAlpha;
		float m_flags;
	};
	static_assert(sizeof(LightParamsRec) == 0x24);

	struct LightRec
	{
		uint32 m_ID;
		uint32 m_continentID;
		float m_gameCoords[3];
		float m_gameFalloffStart;
		float m_gameFalloffEnd;
		uint32 m_lightParamsID[8];
	};
	static_assert(sizeof(LightRec) == 0x3C);

	struct LightSkyboxRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_flags;
	};
	static_assert(sizeof(LightSkyboxRec) == 0xC);

	struct LiquidMaterialRec
	{
		uint32 m_ID;
		uint32 m_lvf;
		uint32 m_flags;
	};
	static_assert(sizeof(LiquidMaterialRec) == 0xC);

	struct LiquidTypeRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_flags;
		uint32 m_soundBank;
		uint32 m_soundID;
		uint32 m_spellID;
		float m_maxDarkenDepth;
		float m_fogDarkenIntensity;
		float m_ambDarkenIntensity;
		float m_dirDarkenIntensity;
		uint32 m_lightID;
		float m_particleScale;
		uint32 m_particleMovement;
		uint32 m_particleTexSlots;
		uint32 m_materialID;
		const char* m_texture[6];
		uint32 m_color[2];
		float m_float[18];
		uint32 m_int[4];
	};
	static_assert(sizeof(LiquidTypeRec) == 0xB4);

	struct LoadingScreensRec
	{
		uint32 m_ID;
		const char* m_name;
		const char* m_fileName;
		uint32 m_hasWideScreen;
	};
	static_assert(sizeof(LoadingScreensRec) == 0x10);

	struct LoadingScreenTaxiSplinesRec
	{
		uint32 m_ID;
		uint32 m_pathID;
		float m_locx[8];
		float m_locy[8];
		uint32 m_legIndex;
	};
	static_assert(sizeof(LoadingScreenTaxiSplinesRec) == 0x4C);

	struct LockRec
	{
		uint32 m_ID;
		uint32 m_type[8];
		uint32 m_index[8];
		uint32 m_skill[8];
		uint32 m_action[8];
	};
	static_assert(sizeof(LockRec) == 0x84);

	struct LockTypeRec
	{
		uint32 m_ID;
		const char* m_name;
		const char* m_resourceName;
		const char* m_verb;
		const char* m_cursorName;
	};
	static_assert(sizeof(LockTypeRec) == 0x14);

	struct MailTemplateRec
	{
		uint32 m_ID;
		const char* m_subject;
		const char* m_body;
	};
	static_assert(sizeof(MailTemplateRec) == 0xC);

	struct MapDifficultyRec
	{
		uint32 m_ID;
		uint32 m_mapID;
		uint32 m_difficulty;
		const char* m_message;
		uint32 m_raidDuration;
		uint32 m_maxPlayers;
		const char* m_difficultystring;
	};
	static_assert(sizeof(MapDifficultyRec) == 0x1C);

	struct MapRec
	{
		uint32 m_ID;
		const char* m_directory;
		uint32 m_instanceType;
		uint32 m_flags;
		uint32 m_pvp;
		const char* m_mapName;
		uint32 m_areaTableID;
		const char* m_mapDescription0;
		const char* m_mapDescription1;
		uint32 m_loadingScreenID;
		float m_minimapIconScale;
		int32 m_corpseMapID;
		float m_corpse[2];
		int32 m_timeOfDayOverride;
		uint32 m_expansionID;
		uint32 m_raidOffset;
		uint32 m_maxPlayers;
	};
	static_assert(sizeof(MapRec) == 0x48);

	struct MaterialRec
	{
		uint32 m_ID;
		uint32 m_flags;
		uint32 m_foleySoundID;
		uint32 m_sheatheSoundID;
		uint32 m_unsheatheSoundID;
	};
	static_assert(sizeof(MaterialRec) == 0x14);

	struct MovieFileDataRec
	{
		uint32 m_fileDataID;
		uint32 m_resolution;
	};
	static_assert(sizeof(MovieFileDataRec) == 0x8);

	struct MovieRec
	{
		uint32 m_ID;
		const char* m_filename;
		uint32 m_volume;
	};
	static_assert(sizeof(MovieRec) == 0xC);

	struct MovieVariationRec
	{
		uint32 m_ID;
		uint32 m_movieID;
		uint32 m_fileDataID;
	};
	static_assert(sizeof(MovieVariationRec) == 0xC);

	struct NameGenRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_raceID;
		uint32 m_sex;
	};
	static_assert(sizeof(NameGenRec) == 0x10);

	struct NamesProfanityRec
	{
		uint32 m_ID;
		const char* m_name;
		int32 m_language;
	};
	static_assert(sizeof(NamesProfanityRec) == 0xC);

	struct NamesReservedRec
	{
		uint32 m_ID;
		const char* m_name;
		int32 m_language;
	};
	static_assert(sizeof(NamesReservedRec) == 0xC);

	struct NPCSoundsRec
	{
		uint32 m_ID;
		int32 m_soundID[4];
	};
	static_assert(sizeof(NPCSoundsRec) == 0x14);

	struct ObjectEffectGroupRec
	{
		uint32 m_ID;
		const char* m_name;
	};
	static_assert(sizeof(ObjectEffectGroupRec) == 0x8);

	struct ObjectEffectModifierRec
	{
		uint32 m_ID;
		uint32 m_inputType;
		uint32 m_mapType;
		uint32 m_outputType;
		float m_param[4];
	};
	static_assert(sizeof(ObjectEffectModifierRec) == 0x20);

	struct ObjectEffectPackageElemRec
	{
		uint32 m_ID;
		uint32 m_objectEffectPackageID;
		uint32 m_objectEffectGroupID;
		uint32 m_stateType;
	};
	static_assert(sizeof(ObjectEffectPackageElemRec) == 0x10);

	struct ObjectEffectPackageRec
	{
		uint32 m_ID;
		const char* m_name;
	};
	static_assert(sizeof(ObjectEffectPackageRec) == 0x8);

	struct ObjectEffectRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_objectEffectGroupID;
		uint32 m_triggerType;
		uint32 m_eventType;
		uint32 m_effectRecType;
		uint32 m_effectRecID;
		uint32 m_attachment;
		float m_offset[3];
		uint32 m_objectEffectModifierID;
	};
	static_assert(sizeof(ObjectEffectRec) == 0x30);

	struct OverrideSpellDataRec
	{
		uint32 m_ID;
		uint32 m_spells[10];
		uint32 m_flags;
	};
	static_assert(sizeof(OverrideSpellDataRec) == 0x30);

	struct PackageRec
	{
		uint32 m_ID;
		const char* m_icon;
		uint32 m_cost;
		const char* m_name;
	};
	static_assert(sizeof(PackageRec) == 0x10);

	struct PageTextMaterialRec
	{
		uint32 m_ID;
		const char* m_name;
	};
	static_assert(sizeof(PageTextMaterialRec) == 0x8);

	struct PaperDollItemFrameRec
	{
		const char* m_itemButtonName;
		const char* m_slotIcon;
		uint32 m_slotNumber;
		int32 m_generatedID;
	};
	static_assert(sizeof(PaperDollItemFrameRec) == 0x10);

	struct ParticleColorRec
	{
		uint32 m_ID;
		int32 m_start[3];
		int32 m_mid[3];
		int32 m_end[3];
	};
	static_assert(sizeof(ParticleColorRec) == 0x28);

	struct PetPersonalityRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_happinessThreshold[3];
		float m_happinessDamage[3];
	};
	static_assert(sizeof(PetPersonalityRec) == 0x20);

	struct PowerDisplayRec
	{
		uint32 m_ID;
		uint32 m_actualType;
		const char* m_globalStringBaseTag;
		uint8 m_red;
		uint8 m_green;
		uint8 m_blue;
		uint8 m_padF[1];
	};
	static_assert(sizeof(PowerDisplayRec) == 0x10);

	struct PVPDifficultyRec
	{
		uint32 m_ID;
		uint32 m_mapID;
		uint32 m_rangeIndex;
		uint32 m_minLevel;
		uint32 m_maxLevel;
		uint32 m_difficulty;
	};
	static_assert(sizeof(PVPDifficultyRec) == 0x18);

	struct QuestFactionRewardRec
	{
		uint32 m_ID;
		int32 m_difficulty[10];
	};
	static_assert(sizeof(QuestFactionRewardRec) == 0x2C);

	struct QuestInfoRec
	{
		uint32 m_ID;
		const char* m_infoName;
	};
	static_assert(sizeof(QuestInfoRec) == 0x8);

	struct QuestSortRec
	{
		uint32 m_ID;
		const char* m_sortName;
	};
	static_assert(sizeof(QuestSortRec) == 0x8);

	struct QuestXPRec
	{
		uint32 m_ID;
		uint32 m_difficulty[10];
	};
	static_assert(sizeof(QuestXPRec) == 0x2C);

	struct RandPropPointsRec
	{
		uint32 m_ID;
		uint32 m_epic[5];
		uint32 m_superior[5];
		uint32 m_good[5];
	};
	static_assert(sizeof(RandPropPointsRec) == 0x40);

	struct ResistancesRec
	{
		uint32 m_ID;
		uint32 m_flags;
		uint32 m_fizzleSoundID;
		const char* m_name;
	};
	static_assert(sizeof(ResistancesRec) == 0x10);

	struct ScalingStatDistributionRec
	{
		uint32 m_ID;
		int32 m_statID[10];
		uint32 m_bonus[10];
		uint32 m_maxlevel;
	};
	static_assert(sizeof(ScalingStatDistributionRec) == 0x58);

	struct ScalingStatValuesRec
	{
		uint32 m_ID;
		uint32 m_charlevel;
		uint32 m_shoulderBudget;
		uint32 m_trinketBudget;
		uint32 m_weaponBudget1h;
		uint32 m_rangedBudget;
		uint32 m_clothShoulderArmor;
		uint32 m_leatherShoulderArmor;
		uint32 m_mailShoulderArmor;
		uint32 m_plateShoulderArmor;
		uint32 m_weaponDps1h;
		uint32 m_weaponDps2h;
		uint32 m_spellcasterDps1h;
		uint32 m_spellcasterDps2h;
		uint32 m_rangedDps;
		uint32 m_wandDps;
		uint32 m_spellPower;
		uint32 m_primaryBudget;
		uint32 m_tertiaryBudget;
		uint32 m_clothCloakArmor;
		uint32 m_clothChestArmor;
		uint32 m_leatherChestArmor;
		uint32 m_mailChestArmor;
		uint32 m_plateChestArmor;
	};
	static_assert(sizeof(ScalingStatValuesRec) == 0x60);

	struct ScreenEffectRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_effect;
		uint32 m_param[4];
		int32 m_lightParamsID;
		uint32 m_soundAmbienceID;
		uint32 m_zoneMusicID;
	};
	static_assert(sizeof(ScreenEffectRec) == 0x28);

	struct ServerMessagesRec
	{
		uint32 m_ID;
		const char* m_text;
	};
	static_assert(sizeof(ServerMessagesRec) == 0x8);

	struct SheatheSoundLookupsRec
	{
		uint32 m_ID;
		uint32 m_classID;
		uint32 m_subclassID;
		uint32 m_material;
		uint32 m_checkMaterial;
		uint32 m_sheatheSound;
		uint32 m_unsheatheSound;
	};
	static_assert(sizeof(SheatheSoundLookupsRec) == 0x1C);

	struct SkillCostsDataRec
	{
		uint32 m_ID;
		uint32 m_skillCostsID;
		uint32 m_cost[3];
	};
	static_assert(sizeof(SkillCostsDataRec) == 0x14);

	struct SkillLineAbilityRec
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
	};
	static_assert(sizeof(SkillLineAbilityRec) == 0x38);

	struct SkillLineCategoryRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_sortIndex;
	};
	static_assert(sizeof(SkillLineCategoryRec) == 0xC);

	struct SkillLineRec
	{
		uint32 m_ID;
		int32 m_categoryID;
		uint32 m_skillCostsID;
		const char* m_displayName;
		const char* m_description;
		uint32 m_spellIconID;
		const char* m_alternateVerb;
		uint32 m_canLink;
	};
	static_assert(sizeof(SkillLineRec) == 0x20);

	struct SkillRaceClassInfoRec
	{
		uint32 m_ID;
		uint32 m_skillID;
		uint32 m_raceMask;
		uint32 m_classMask;
		uint32 m_flags;
		uint32 m_minLevel;
		uint32 m_skillTierID;
		uint32 m_skillCostIndex;
	};
	static_assert(sizeof(SkillRaceClassInfoRec) == 0x20);

	struct SkillTiersRec
	{
		uint32 m_ID;
		uint32 m_cost[16];
		uint32 m_value[16];
	};
	static_assert(sizeof(SkillTiersRec) == 0x84);

	struct SoundAmbienceRec
	{
		uint32 m_ID;
		uint32 m_ambienceID[2];
	};
	static_assert(sizeof(SoundAmbienceRec) == 0xC);

	struct SoundEmittersRec
	{
		uint32 m_ID;
		float m_position[3];
		float m_direction[3];
		uint32 m_soundEntryAdvancedID;
		uint32 m_mapID;
		const char* m_name;
	};
	static_assert(sizeof(SoundEmittersRec) == 0x28);

	struct SoundEntriesAdvancedRec
	{
		uint32 m_ID;
		uint32 m_soundEntryID;
		float m_innerRadius2d;
		uint32 m_timeA;
		uint32 m_timeB;
		uint32 m_timeC;
		uint32 m_timeD;
		uint32 m_randomOffsetRange;
		uint32 m_usage;
		uint32 m_timeIntervalMin;
		uint32 m_timeIntervalMax;
		uint32 m_volumeSliderCategory;
		float m_duckToSfx;
		float m_duckToMusic;
		float m_duckToAmbience;
		float m_innerRadiusOfInfluence;
		float m_outerRadiusOfInfluence;
		uint32 m_timeToDuck;
		uint32 m_timeToUnduck;
		float m_insideAngle;
		float m_outsideAngle;
		float m_outsideVolume;
		float m_outerRadius2d;
		const char* m_name;
	};
	static_assert(sizeof(SoundEntriesAdvancedRec) == 0x60);

	struct SoundEntriesRec
	{
		uint32 m_ID;
		uint32 m_soundType;
		const char* m_name;
		const char* m_file[10];
		uint32 m_freq[10];
		const char* m_directoryBase;
		float m_volumeFloat;
		uint32 m_flags;
		float m_minDistance;
		float m_distanceCutoff;
		uint32 m_eaxdef;
		uint32 m_soundEntriesAdvancedID;
	};
	static_assert(sizeof(SoundEntriesRec) == 0x78);

	struct SoundFilterElemRec
	{
		uint32 m_ID;
		uint32 m_soundFilterID;
		uint32 m_orderIndex;
		uint32 m_filterType;
		float m_params[9];
	};
	static_assert(sizeof(SoundFilterElemRec) == 0x34);

	struct SoundFilterRec
	{
		uint32 m_ID;
		const char* m_name;
	};
	static_assert(sizeof(SoundFilterRec) == 0x8);

	struct SoundProviderPreferencesRec
	{
		uint32 m_ID;
		const char* m_description;
		uint32 m_flags;
		int32 m_eaxenvironmentSelection;
		float m_eaxdecayTime;
		float m_eax2EnvironmentSize;
		float m_eax2EnvironmentDiffusion;
		int32 m_eax2Room;
		int32 m_eax2RoomHf;
		float m_eax2DecayHfratio;
		int32 m_eax2Reflections;
		float m_eax2ReflectionsDelay;
		int32 m_eax2Reverb;
		float m_eax2ReverbDelay;
		float m_eax2RoomRolloff;
		float m_eax2AirAbsorption;
		uint32 m_eax3RoomLf;
		float m_eax3DecayLfratio;
		float m_eax3EchoTime;
		float m_eax3EchoDepth;
		float m_eax3ModulationTime;
		float m_eax3ModulationDepth;
		float m_eax3Hfreference;
		float m_eax3Lfreference;
	};
	static_assert(sizeof(SoundProviderPreferencesRec) == 0x60);

	struct SoundSamplePreferencesRec
	{
		uint32 m_ID;
		uint32 m_field_0_6_0_3592_001;
		uint32 m_field_0_6_0_3592_002;
		uint32 m_eax2SampleRoom;
		uint32 m_field_0_6_0_3592_004;
		uint32 m_field_0_6_0_3592_005;
		float m_field_0_6_0_3592_006;
		uint32 m_field_0_6_0_3592_007;
		float m_eax2SampleOcclusionLfratio;
		float m_eax2SampleOcclusionRoomRatio;
		uint32 m_field_0_6_0_3592_010;
		float m_eax1EffectLevel;
		uint32 m_field_0_6_0_3592_012;
		float m_field_0_6_0_3592_013;
		float m_eax3SampleExclusion;
		float m_field_0_6_0_3592_015;
		uint32 m_field_0_6_0_3592_016;
	};
	static_assert(sizeof(SoundSamplePreferencesRec) == 0x44);

	struct SoundWaterTypeRec
	{
		uint32 m_ID;
		uint32 m_soundType;
		uint32 m_soundSubtype;
		uint32 m_soundID;
	};
	static_assert(sizeof(SoundWaterTypeRec) == 0x10);

	struct SpamMessagesRec
	{
		uint32 m_ID;
		const char* m_text;
	};
	static_assert(sizeof(SpamMessagesRec) == 0x8);

	struct SpellCastTimesRec
	{
		uint32 m_ID;
		int32 m_base;
		int32 m_perLevel;
		int32 m_minimum;
	};
	static_assert(sizeof(SpellCastTimesRec) == 0x10);

	struct SpellCategoryRec
	{
		uint32 m_ID;
		uint32 m_flags;
	};
	static_assert(sizeof(SpellCategoryRec) == 0x8);

	struct SpellChainEffectsRec
	{
		uint32 m_ID;
		float m_avgSegLen;
		float m_width;
		float m_noiseScale;
		float m_texCoordScale;
		uint32 m_segDuration;
		uint32 m_segDelay;
		const char* m_texture;
		uint32 m_flags;
		uint32 m_jointCount;
		float m_jointOffsetRadius;
		uint32 m_jointsPerMinorJoint;
		uint32 m_minorJointsPerMajorJoint;
		float m_minorJointScale;
		float m_majorJointScale;
		float m_jointMoveSpeed;
		float m_jointSmoothness;
		float m_minDurationBetweenJointJumps;
		float m_maxDurationBetweenJointJumps;
		float m_waveHeight;
		float m_waveFreq;
		float m_waveSpeed;
		float m_minWaveAngle;
		float m_maxWaveAngle;
		float m_minWaveSpin;
		float m_maxWaveSpin;
		float m_arcHeight;
		float m_minArcAngle;
		float m_maxArcAngle;
		float m_minArcSpin;
		float m_maxArcSpin;
		float m_delayBetweenEffects;
		float m_minFlickerOnDuration;
		float m_maxFlickerOnDuration;
		float m_minFlickerOffDuration;
		float m_maxFlickerOffDuration;
		float m_pulseSpeed;
		float m_pulseOnLength;
		float m_pulseFadeLength;
		uint8 m_alpha;
		uint8 m_red;
		uint8 m_green;
		uint8 m_blue;
		uint8 m_blendMode;
		uint8 m_padA1[3];
		const char* m_combo;
		uint32 m_renderLayer;
		float m_textureLength;
		float m_wavePhase;
	};
	static_assert(sizeof(SpellChainEffectsRec) == 0xB4);

	struct SpellDescriptionVariablesRec
	{
		uint32 m_ID;
		const char* m_variables;
	};
	static_assert(sizeof(SpellDescriptionVariablesRec) == 0x8);

	struct SpellDifficultyRec
	{
		uint32 m_ID;
		int32 m_difficultySpellID[4];
	};
	static_assert(sizeof(SpellDifficultyRec) == 0x14);

	struct SpellDispelTypeRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_mask;
		uint32 m_immunityPossible;
		const char* m_internalName;
	};
	static_assert(sizeof(SpellDispelTypeRec) == 0x14);

	struct SpellDurationRec
	{
		uint32 m_ID;
		int32 m_duration;
		uint32 m_durationPerLevel;
		int32 m_maxDuration;
	};
	static_assert(sizeof(SpellDurationRec) == 0x10);

	struct SpellEffectCameraShakesRec
	{
		uint32 m_ID;
		uint32 m_cameraShake[3];
	};
	static_assert(sizeof(SpellEffectCameraShakesRec) == 0x10);

	struct SpellFocusObjectRec
	{
		uint32 m_ID;
		const char* m_name;
	};
	static_assert(sizeof(SpellFocusObjectRec) == 0x8);

	struct SpellIconRec
	{
		uint32 m_ID;
		const char* m_textureFilename;
	};
	static_assert(sizeof(SpellIconRec) == 0x8);

	struct SpellItemEnchantmentConditionRec
	{
		uint32 m_ID;
		uint8 m_ltOperandType[5];
		uint8 m_pad9[3];
		uint32 m_ltOperand[5];
		uint8 m_operator[5];
		uint8 m_rtOperandType[5];
		uint8 m_pad2A[2];
		uint32 m_rtOperand[5];
		uint8 m_logic[5];
		uint8 m_pad45[3];
	};
	static_assert(sizeof(SpellItemEnchantmentConditionRec) == 0x48);

	struct SpellItemEnchantmentRec
	{
		uint32 m_ID;
		uint32 m_charges;
		uint32 m_effect[3];
		int32 m_effectPointsMin[3];
		int32 m_effectPointsMax[3];
		uint32 m_effectArg[3];
		const char* m_name;
		int32 m_itemVisual;
		uint32 m_flags;
		uint32 m_srcItemID;
		uint32 m_conditionID;
		uint32 m_requiredSkillID;
		uint32 m_requiredSkillRank;
		uint32 m_minLevel;
	};
	static_assert(sizeof(SpellItemEnchantmentRec) == 0x58);

	struct SpellMechanicRec
	{
		uint32 m_ID;
		const char* m_stateName;
	};
	static_assert(sizeof(SpellMechanicRec) == 0x8);

	struct SpellMissileMotionRec
	{
		uint32 m_ID;
		const char* m_name;
		const char* m_scriptBody;
		uint32 m_flags;
		uint32 m_missileCount;
	};
	static_assert(sizeof(SpellMissileMotionRec) == 0x14);

	struct SpellMissileRec
	{
		uint32 m_ID;
		uint32 m_flags;
		float m_defaultPitchMin;
		float m_defaultPitchMax;
		float m_defaultSpeedMin;
		float m_defaultSpeedMax;
		float m_randomizeFacingMin;
		float m_randomizeFacingMax;
		float m_randomizePitchMin;
		float m_randomizePitchMax;
		float m_randomizeSpeedMin;
		float m_randomizeSpeedMax;
		float m_gravity;
		float m_maxDuration;
		float m_collisionRadius;
	};
	static_assert(sizeof(SpellMissileRec) == 0x3C);

	struct SpellRadiusRec
	{
		uint32 m_ID;
		float m_radius;
		float m_radiusPerLevel;
		float m_radiusMax;
	};
	static_assert(sizeof(SpellRadiusRec) == 0x10);

	struct SpellRangeRec
	{
		uint32 m_ID;
		float m_rangeMin[2];
		float m_rangeMax[2];
		uint32 m_flags;
		const char* m_displayName;
		const char* m_displayNameShort;
	};
	static_assert(sizeof(SpellRangeRec) == 0x20);

	struct SpellRec
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
		int32 m_powerType;
		uint32 m_manaCost;
		uint32 m_manaCostPerLevel;
		uint32 m_manaPerSecond;
		uint32 m_manaPerSecondPerLevel;
		uint32 m_rangeIndex;
		float m_speed;
		uint32 m_modalNextSpell;
		uint32 m_cumulativeAura;
		uint32 m_totem[2];
		int32 m_reagent[8];
		int32 m_reagentCount[8];
		int32 m_equippedItemClass;
		uint32 m_equippedItemSubclass;
		uint32 m_equippedItemInvTypes;
		uint32 m_effect[3];
		int32 m_effectDieSides[3];
		float m_effectRealPointsPerLevel[3];
		int32 m_effectBasePoints[3];
		uint32 m_effectMechanic[3];
		uint32 m_implicitTargetA[3];
		uint32 m_implicitTargetB[3];
		uint32 m_effectRadiusIndex[3];
		uint32 m_effectAura[3];
		uint32 m_effectAuraPeriod[3];
		float m_effectAmplitude[3];
		uint32 m_effectChainTargets[3];
		uint32 m_effectItemType[3];
		int32 m_effectMiscValue[3];
		int32 m_effectMiscValueB[3];
		int32 m_effectTriggerSpell[3];
		float m_effectPointsPerCombo[3];
		uint32 m_effectSpellClassMaskA[3];
		uint32 m_effectSpellClassMaskB[3];
		uint32 m_effectSpellClassMaskC[3];
		uint32 m_spellVisualID[2];
		uint32 m_spellIconID;
		uint32 m_activeIconID;
		uint32 m_spellPriority;
		const char* m_name;
		const char* m_nameSubtext;
		const char* m_description;
		const char* m_auraDescription;
		uint32 m_manaCostPct;
		uint32 m_startRecoveryCategory;
		uint32 m_startRecoveryTime;
		uint32 m_maxTargetLevel;
		uint32 m_spellClassSet;
		uint32 m_spellClassMask[3];
		uint32 m_maxTargets;
		uint32 m_defenseType;
		uint32 m_preventionType;
		int32 m_stanceBarOrder;
		float m_effectChainAmplitude[3];
		uint32 m_minFactionID;
		uint32 m_minReputation;
		uint32 m_requiredAuraVision;
		uint32 m_requiredTotemCategoryID[2];
		int32 m_requiredAreasID;
		uint32 m_schoolMask;
		uint32 m_runeCostID;
		uint32 m_spellMissileID;
		int32 m_powerDisplayID;
		float m_effectBonusCoefficient[3];
		int32 m_descriptionVariablesID;
		uint32 m_difficulty;
	};
	static_assert(sizeof(SpellRec) == 0x2A8);

	struct SpellRuneCostRec
	{
		uint32 m_ID;
		uint32 m_blood;
		uint32 m_unholy;
		uint32 m_frost;
		uint32 m_runicPower;
	};
	static_assert(sizeof(SpellRuneCostRec) == 0x14);

	struct SpellShapeshiftFormRec
	{
		uint32 m_ID;
		uint32 m_bonusActionBar;
		const char* m_name;
		uint32 m_flags;
		int32 m_creatureType;
		uint32 m_attackIconID;
		uint32 m_combatRoundTime;
		uint32 m_creatureDisplayID[4];
		uint32 m_presetSpellID[8];
	};
	static_assert(sizeof(SpellShapeshiftFormRec) == 0x4C);

	struct SpellVisualEffectNameRec
	{
		uint32 m_ID;
		const char* m_name;
		const char* m_fileName;
		float m_areaEffectSize;
		float m_scale;
		float m_minAllowedScale;
		float m_maxAllowedScale;
	};
	static_assert(sizeof(SpellVisualEffectNameRec) == 0x1C);

	struct SpellVisualKitAreaModelRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_enumID;
	};
	static_assert(sizeof(SpellVisualKitAreaModelRec) == 0xC);

	struct SpellVisualKitModelAttachRec
	{
		uint32 m_ID;
		uint32 m_parentSpellVisualKitID;
		uint32 m_spellVisualEffectNameID;
		int32 m_attachmentID;
		float m_offset[3];
		float m_yaw;
		float m_pitch;
		float m_roll;
	};
	static_assert(sizeof(SpellVisualKitModelAttachRec) == 0x28);

	struct SpellVisualKitRec
	{
		uint32 m_ID;
		int32 m_startAnimID;
		int32 m_animID;
		int32 m_headEffect;
		int32 m_chestEffect;
		int32 m_baseEffect;
		int32 m_leftHandEffect;
		int32 m_rightHandEffect;
		int32 m_breathEffect;
		int32 m_leftWeaponEffect;
		int32 m_rightWeaponEffect;
		int32 m_specialEffect[3];
		int32 m_worldEffect;
		int32 m_soundID;
		uint32 m_shakeID;
		int32 m_charProc[4];
		float m_charParamZero[4];
		float m_charParamOne[4];
		float m_charParamTwo[4];
		float m_charParamThree[4];
		uint32 m_flags;
	};
	static_assert(sizeof(SpellVisualKitRec) == 0x98);

	struct SpellVisualRec
	{
		uint32 m_ID;
		uint32 m_precastKit;
		uint32 m_castKit;
		uint32 m_impactKit;
		uint32 m_stateKit;
		uint32 m_stateDoneKit;
		uint32 m_channelKit;
		uint32 m_hasMissile;
		int32 m_missileModel;
		uint32 m_missilePathType;
		uint32 m_missileDestinationAttachment;
		int32 m_missileSound;
		int32 m_animEventSoundID;
		uint32 m_flags;
		uint32 m_casterImpactKit;
		uint32 m_targetImpactKit;
		int32 m_missileAttachment;
		int32 m_missileFollowGroundHeight;
		uint32 m_missileFollowGroundDropSpeed;
		uint32 m_missileFollowGroundApproach;
		uint32 m_missileFollowGroundFlags;
		uint32 m_missileMotion;
		uint32 m_missileTargetingKit;
		uint32 m_instantAreaKit;
		uint32 m_impactAreaKit;
		uint32 m_persistentAreaKit;
		float m_missileCastOffset[3];
		float m_missileImpactOffset[3];
	};
	static_assert(sizeof(SpellVisualRec) == 0x80);

	struct StableSlotPricesRec
	{
		uint32 m_ID;
		uint32 m_cost;
	};
	static_assert(sizeof(StableSlotPricesRec) == 0x8);

	struct Startup_StringsRec
	{
		uint32 m_ID;
		const char* m_name;
		const char* m_message;
	};
	static_assert(sizeof(Startup_StringsRec) == 0xC);

	struct StationeryRec
	{
		uint32 m_ID;
		uint32 m_itemID;
		const char* m_texture;
		uint32 m_flags;
	};
	static_assert(sizeof(StationeryRec) == 0x10);

	struct StringLookupsRec
	{
		uint32 m_ID;
		const char* m_string;
	};
	static_assert(sizeof(StringLookupsRec) == 0x8);

	struct SummonPropertiesRec
	{
		uint32 m_ID;
		uint32 m_control;
		uint32 m_faction;
		uint32 m_title;
		uint32 m_slot;
		uint32 m_flags;
	};
	static_assert(sizeof(SummonPropertiesRec) == 0x18);

	struct TalentRec
	{
		uint32 m_ID;
		uint32 m_tabID;
		uint32 m_tierID;
		uint32 m_columnIndex;
		uint32 m_spellRank[9];
		uint32 m_prereqTalent[3];
		uint32 m_prereqRank[3];
		uint32 m_flags;
		uint32 m_requiredSpellID;
		uint32 m_categoryMask[2];
	};
	static_assert(sizeof(TalentRec) == 0x5C);

	struct TalentTabRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_spellIconID;
		uint32 m_raceMask;
		uint32 m_classMask;
		uint32 m_categoryEnumID;
		uint32 m_orderIndex;
		const char* m_backgroundFile;
	};
	static_assert(sizeof(TalentTabRec) == 0x20);

	struct TaxiNodesRec
	{
		uint32 m_ID;
		uint32 m_continentID;
		float m_pos[3];
		const char* m_name;
		uint32 m_mountCreatureID[2];
	};
	static_assert(sizeof(TaxiNodesRec) == 0x20);

	struct TaxiPathNodeRec
	{
		uint32 m_ID;
		uint32 m_pathID;
		uint32 m_nodeIndex;
		uint32 m_continentID;
		float m_loc[3];
		uint32 m_flags;
		uint32 m_delay;
		uint32 m_arrivalEventID;
		uint32 m_departureEventID;
	};
	static_assert(sizeof(TaxiPathNodeRec) == 0x2C);

	struct TaxiPathRec
	{
		uint32 m_ID;
		uint32 m_fromTaxiNode;
		int32 m_toTaxiNode;
		uint32 m_cost;
	};
	static_assert(sizeof(TaxiPathRec) == 0x10);

	struct TeamContributionPointsRec
	{
		uint32 m_ID;
		float m_data;
	};
	static_assert(sizeof(TeamContributionPointsRec) == 0x8);

	struct TerrainTypeRec
	{
		uint32 m_terrainID;
		const char* m_terrainDesc;
		uint32 m_footstepSprayRun;
		uint32 m_footstepSprayWalk;
		uint32 m_soundID;
		uint32 m_flags;
		int32 m_generatedID;
	};
	static_assert(sizeof(TerrainTypeRec) == 0x1C);

	struct TerrainTypeSoundsRec
	{
		uint32 m_ID;
	};
	static_assert(sizeof(TerrainTypeSoundsRec) == 0x4);

	struct TotemCategoryRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_totemCategoryType;
		uint32 m_totemCategoryMask;
	};
	static_assert(sizeof(TotemCategoryRec) == 0x10);

	struct TransportAnimationRec
	{
		uint32 m_ID;
		uint32 m_transportID;
		uint32 m_timeIndex;
		float m_pos[3];
		uint32 m_sequenceID;
	};
	static_assert(sizeof(TransportAnimationRec) == 0x1C);

	struct TransportPhysicsRec
	{
		uint32 m_ID;
		float m_waveAmp;
		float m_waveTimeScale;
		float m_rollAmp;
		float m_rollTimeScale;
		float m_pitchAmp;
		float m_pitchTimeScale;
		float m_maxBank;
		float m_maxBankTurnSpeed;
		float m_speedDampThresh;
		float m_speedDamp;
	};
	static_assert(sizeof(TransportPhysicsRec) == 0x2C);

	struct TransportRotationRec
	{
		uint32 m_ID;
		uint32 m_gameObjectsID;
		uint32 m_timeIndex;
		float m_rot[4];
	};
	static_assert(sizeof(TransportRotationRec) == 0x1C);

	struct UISoundLookupsRec
	{
		uint32 m_ID;
		uint32 m_soundID;
		const char* m_soundName;
	};
	static_assert(sizeof(UISoundLookupsRec) == 0xC);

	struct UnitBloodLevelsRec
	{
		uint32 m_ID;
		uint32 m_violencelevel[3];
	};
	static_assert(sizeof(UnitBloodLevelsRec) == 0x10);

	struct UnitBloodRec
	{
		uint32 m_ID;
		uint32 m_combatBloodSpurtFront[2];
		uint32 m_combatBloodSpurtBack[2];
		const char* m_groundBlood[5];
	};
	static_assert(sizeof(UnitBloodRec) == 0x28);

	struct VehicleRec
	{
		uint32 m_ID;
		uint32 m_flags;
		float m_turnSpeed;
		float m_pitchSpeed;
		float m_pitchMin;
		float m_pitchMax;
		uint32 m_seatID[8];
		float m_mouseLookOffsetPitch;
		float m_cameraFadeDistScalarMin;
		float m_cameraFadeDistScalarMax;
		float m_cameraPitchOffset;
		float m_facingLimitRight;
		float m_facingLimitLeft;
		float m_msslTrgtTurnLingering;
		float m_msslTrgtPitchLingering;
		float m_msslTrgtMouseLingering;
		float m_msslTrgtEndOpacity;
		float m_msslTrgtArcSpeed;
		float m_msslTrgtArcRepeat;
		float m_msslTrgtArcWidth;
		float m_msslTrgtImpactRadius[2];
		const char* m_msslTrgtArcTexture;
		const char* m_msslTrgtImpactTexture;
		const char* m_msslTrgtImpactModel[2];
		float m_cameraYawOffset;
		uint32 m_uiLocomotionType;
		float m_msslTrgtImpactTexRadius;
		uint32 m_vehicleUiindicatorID;
		int32 m_powerDisplayID[3];
	};
	static_assert(sizeof(VehicleRec) == 0xA0);

	struct VehicleSeatRec
	{
		uint32 m_ID;
		uint32 m_flags;
		int32 m_attachmentID;
		float m_attachmentOffset[3];
		float m_enterPreDelay;
		float m_enterSpeed;
		float m_enterGravity;
		float m_enterMinDuration;
		float m_enterMaxDuration;
		float m_enterMinArcHeight;
		float m_enterMaxArcHeight;
		int32 m_enterAnimStart;
		int32 m_enterAnimLoop;
		int32 m_rideAnimStart;
		int32 m_rideAnimLoop;
		int32 m_rideUpperAnimStart;
		int32 m_rideUpperAnimLoop;
		float m_exitPreDelay;
		float m_exitSpeed;
		float m_exitGravity;
		float m_exitMinDuration;
		float m_exitMaxDuration;
		float m_exitMinArcHeight;
		float m_exitMaxArcHeight;
		int32 m_exitAnimStart;
		int32 m_exitAnimLoop;
		int32 m_exitAnimEnd;
		float m_passengerYaw;
		float m_passengerPitch;
		float m_passengerRoll;
		int32 m_passengerAttachmentID;
		int32 m_vehicleEnterAnim;
		int32 m_vehicleExitAnim;
		int32 m_vehicleRideAnimLoop;
		int32 m_field_3_3_5_12213_034;
		int32 m_vehicleExitAnimBone;
		int32 m_vehicleEnterAnimBone;
		float m_vehicleEnterAnimDelay;
		float m_vehicleExitAnimDelay;
		uint32 m_vehicleAbilityDisplay;
		uint32 m_enterUisoundID;
		int32 m_exitUISoundID;
		int32 m_uiSkin;
		uint32 m_flagsB;
		float m_cameraEnteringDelay;
		float m_cameraEnteringDuration;
		float m_cameraExitingDelay;
		float m_cameraExitingDuration;
		float m_cameraOffsetX;
		float m_cameraOffsetY;
		float m_cameraOffsetZ;
		float m_cameraPosChaseRate;
		float m_cameraFacingChaseRate;
		float m_cameraEnteringZoom;
		float m_cameraSeatZoomMin;
		float m_cameraSeatZoomMax;
	};
	static_assert(sizeof(VehicleSeatRec) == 0xE8);

	struct VehicleUIIndicatorRec
	{
		uint32 m_ID;
		const char* m_backgroundTexture;
	};
	static_assert(sizeof(VehicleUIIndicatorRec) == 0x8);

	struct VehicleUIIndSeatRec
	{
		uint32 m_ID;
		uint32 m_vehicleUiindicatorID;
		uint32 m_virtualSeatIndex;
		float m_xpos;
		float m_ypos;
	};
	static_assert(sizeof(VehicleUIIndSeatRec) == 0x14);

	struct VideoHardwareRec
	{
		uint32 m_ID;
		uint32 m_vendorID;
		uint32 m_deviceID;
		uint32 m_farclipIdx;
		uint32 m_terrainLoddistIdx;
		uint32 m_terrainShadowLod;
		uint32 m_detailDoodadDensityIdx;
		uint32 m_detailDoodadAlpha;
		uint32 m_animatingDoodadIdx;
		uint32 m_trilinear;
		uint32 m_numLights;
		uint32 m_specularity;
		uint32 m_waterLodidx;
		uint32 m_particleDensityIdx;
		uint32 m_unitDrawDistIdx;
		uint32 m_smallCullDistIdx;
		uint32 m_resolutionIdx;
		uint32 m_baseMipLevel;
		const char* m_oglOverrides;
		const char* m_d3DOverrides;
		uint32 m_fixLag;
		uint32 m_multisample;
		uint32 m_atlasdisable;
	};
	static_assert(sizeof(VideoHardwareRec) == 0x5C);

	struct VocalUISoundsRec
	{
		uint32 m_ID;
		uint32 m_vocalUienum;
		uint32 m_raceID;
		int32 m_normalSoundID[2];
		int32 m_pissedSoundID[2];
	};
	static_assert(sizeof(VocalUISoundsRec) == 0x1C);

	struct WeaponImpactSoundsRec
	{
		uint32 m_ID;
		uint32 m_weaponSubClassID;
		uint32 m_parrySoundType;
		uint32 m_impactSoundID[10];
		uint32 m_critImpactSoundID[10];
	};
	static_assert(sizeof(WeaponImpactSoundsRec) == 0x5C);

	struct WeaponSwingSounds2Rec
	{
		uint32 m_ID;
		uint32 m_swingType;
		uint32 m_crit;
		uint32 m_soundID;
	};
	static_assert(sizeof(WeaponSwingSounds2Rec) == 0x10);

	struct WeatherRec
	{
		uint32 m_ID;
		uint32 m_ambienceID;
		uint32 m_effectType;
		float m_transitionSkyBox;
		float m_effectColor[3];
		const char* m_effectTexture;
	};
	static_assert(sizeof(WeatherRec) == 0x20);

	struct WMOAreaTableRec
	{
		uint32 m_ID;
		uint32 m_wmoid;
		uint32 m_nameSetID;
		int32 m_wmogroupID;
		int32 m_soundProviderPref;
		uint32 m_soundProviderPrefUnderwater;
		uint32 m_ambienceID;
		uint32 m_zoneMusic;
		int32 m_introSound;
		uint32 m_flags;
		uint32 m_areaTableID;
		const char* m_areaName;
	};
	static_assert(sizeof(WMOAreaTableRec) == 0x30);

	struct WorldChunkSoundsRec
	{
		uint32 m_ID;
		uint32 m_chunkX;
		uint32 m_chunkY;
		uint32 m_subchunkX;
		uint32 m_subchunkY;
		uint32 m_zoneIntroMusicID;
		uint32 m_zoneMusicID;
		uint32 m_soundAmbienceID;
		uint32 m_soundProviderPreferencesID;
		int32 m_unread_24;
	};
	static_assert(sizeof(WorldChunkSoundsRec) == 0x28);

	struct WorldMapAreaRec
	{
		uint32 m_ID;
		uint32 m_mapID;
		uint32 m_areaID;
		const char* m_areaName;
		float m_locLeft;
		float m_locRight;
		float m_locTop;
		float m_locBottom;
		int32 m_displayMapID;
		int32 m_defaultDungeonFloor;
		uint32 m_parentWorldMapID;
	};
	static_assert(sizeof(WorldMapAreaRec) == 0x2C);

	struct WorldMapContinentRec
	{
		uint32 m_ID;
		uint32 m_mapID;
		uint32 m_leftBoundary;
		uint32 m_rightBoundary;
		uint32 m_topBoundary;
		uint32 m_bottomBoundary;
		float m_continentOffset[2];
		float m_scale;
		float m_taxiMin[2];
		float m_taxiMax[2];
		uint32 m_worldMapID;
	};
	static_assert(sizeof(WorldMapContinentRec) == 0x38);

	struct WorldMapOverlayRec
	{
		uint32 m_ID;
		uint32 m_mapAreaID;
		uint32 m_areaID[4];
		uint32 m_mapPointX;
		uint32 m_mapPointY;
		const char* m_textureName;
		uint32 m_textureWidth;
		uint32 m_textureHeight;
		uint32 m_offsetX;
		uint32 m_offsetY;
		uint32 m_hitRectTop;
		uint32 m_hitRectLeft;
		uint32 m_hitRectBottom;
		uint32 m_hitRectRight;
	};
	static_assert(sizeof(WorldMapOverlayRec) == 0x44);

	struct WorldMapTransformsRec
	{
		uint32 m_ID;
		uint32 m_mapID;
		float m_regionMin[2];
		float m_regionMax[2];
		uint32 m_newMapID;
		float m_regionOffset[2];
		uint32 m_newDungeonMapID;
	};
	static_assert(sizeof(WorldMapTransformsRec) == 0x28);

	struct WorldSafeLocsRec
	{
		uint32 m_ID;
		uint32 m_continent;
		float m_loc[3];
		const char* m_areaName;
	};
	static_assert(sizeof(WorldSafeLocsRec) == 0x18);

	struct WorldStateUIRec
	{
		uint32 m_ID;
		uint32 m_mapID;
		uint32 m_areaID;
		uint32 m_phaseShift;
		const char* m_icon;
		const char* m_string;
		const char* m_tooltip;
		uint32 m_stateVariable;
		uint32 m_type;
		const char* m_dynamicIcon;
		const char* m_dynamicTooltip;
		const char* m_extendedUi;
		uint32 m_extendedUistateVariable[3];
	};
	static_assert(sizeof(WorldStateUIRec) == 0x3C);

	struct WorldStateZoneSoundsRec
	{
		uint32 m_worldStateID;
		uint32 m_worldStateValue;
		uint32 m_areaID;
		uint32 m_wmoareaID;
		uint32 m_zoneIntroMusicID;
		uint32 m_zoneMusicID;
		uint32 m_soundAmbienceID;
		uint32 m_soundProviderPreferencesID;
		int32 m_generatedID;
	};
	static_assert(sizeof(WorldStateZoneSoundsRec) == 0x24);

	struct ZoneIntroMusicTableRec
	{
		uint32 m_ID;
		const char* m_name;
		uint32 m_soundID;
		uint32 m_priority;
		uint32 m_minDelayMinutes;
	};
	static_assert(sizeof(ZoneIntroMusicTableRec) == 0x14);

	struct ZoneMusicRec
	{
		uint32 m_ID;
		const char* m_setName;
		uint32 m_silenceIntervalMin[2];
		uint32 m_silenceIntervalMax[2];
		uint32 m_sounds[2];
	};
	static_assert(sizeof(ZoneMusicRec) == 0x20);

	using Achievement_CategoryDB = Table<Achievement_CategoryRec, 0x00AD30A4>;
	using Achievement_CriteriaDB = Table<Achievement_CriteriaRec, 0x00AD3080>;
	using AchievementDB = Table<AchievementRec, 0x00AD305C>;
	using AnimationDataDB = Table<AnimationDataRec, 0x00AD30C8>;
	using AreaGroupDB = Table<AreaGroupRec, 0x00AD30EC>;
	using AreaPOIDB = Table<AreaPOIRec, 0x00AD3110>;
	using AreaTableDB = Table<AreaTableRec, 0x00AD3134>;
	using AreaTriggerDB = Table<AreaTriggerRec, 0x00AD3158>;
	using AttackAnimKitsDB = Table<AttackAnimKitsRec, 0x00AD317C>;
	using AttackAnimTypesDB = Table<AttackAnimTypesRec, 0x00AD31A0>;
	using AuctionHouseDB = Table<AuctionHouseRec, 0x00AD31C4>;
	using BankBagSlotPricesDB = Table<BankBagSlotPricesRec, 0x00AD31E8>;
	using BannedAddOnsDB = Table<BannedAddonsRec, 0x00AD320C>;
	using BarberShopStyleDB = Table<BarberShopStyleRec, 0x00AD3230>;
	using BattlemasterListDB = Table<BattlemasterListRec, 0x00AD3254>;
	using CameraShakesDB = Table<CameraShakesRec, 0x00AD3278>;
	using Cfg_CategoriesDB = Table<Cfg_CategoriesRec, 0x00AD329C>;
	using Cfg_ConfigsDB = Table<Cfg_ConfigsRec, 0x00AD32C0>;
	using CharacterFacialHairStylesDB = Table<CharacterFacialHairStylesRec, 0x00AD3398>;
	using CharBaseInfoDB = Table<CharBaseInfoRec, 0x00AD32E4>;
	using CharHairGeosetsDB = Table<CharHairGeosetsRec, 0x00AD3308>;
	using CharSectionsDB = Table<CharSectionsRec, 0x00AD332C>;
	using CharStartOutfitDB = Table<CharStartOutfitRec, 0x00AD3350>;
	using CharTitlesDB = Table<CharTitlesRec, 0x00AD3374>;
	using ChatChannelsDB = Table<ChatChannelsRec, 0x00AD33BC>;
	using ChatProfanityDB = Table<ChatProfanityRec, 0x00AD33E0>;
	using ChrClassesDB = Table<ChrClassesRec, 0x00AD3404>;
	using ChrRacesDB = Table<ChrRacesRec, 0x00AD3428>;
	using CinematicCameraDB = Table<CinematicCameraRec, 0x00AD344C>;
	using CinematicSequencesDB = Table<CinematicSequencesRec, 0x00AD3470>;
	using CreatureDisplayInfoDB = Table<CreatureDisplayInfoRec, 0x00AD34B8>;
	using CreatureDisplayInfoExtraDB = Table<CreatureDisplayInfoExtraRec, 0x00AD3494>;
	using CreatureFamilyDB = Table<CreatureFamilyRec, 0x00AD34DC>;
	using CreatureModelDataDB = Table<CreatureModelDataRec, 0x00AD3500>;
	using CreatureMovementInfoDB = Table<CreatureMovementInfoRec, 0x00AD3524>;
	using CreatureSoundDataDB = Table<CreatureSoundDataRec, 0x00AD3548>;
	using CreatureSpellDataDB = Table<CreatureSpellDataRec, 0x00AD356C>;
	using CreatureTypeDB = Table<CreatureTypeRec, 0x00AD3590>;
	using CurrencyCategoryDB = Table<CurrencyCategoryRec, 0x00AD35D8>;
	using CurrencyTypesDB = Table<CurrencyTypesRec, 0x00AD35B4>;
	using DanceMovesDB = Table<DanceMovesRec, 0x00AD35FC>;
	using DeathThudLookupsDB = Table<DeathThudLookupsRec, 0x00AD3620>;
	using DestructibleModelDataDB = Table<DestructibleModelDataRec, 0x00AD368C>;
	using DungeonEncounterDB = Table<DungeonEncounterRec, 0x00AD36B0>;
	using DungeonMapChunkDB = Table<DungeonMapChunkRec, 0x00AD36F8>;
	using DungeonMapDB = Table<DungeonMapRec, 0x00AD36D4>;
	using DurabilityCostsDB = Table<DurabilityCostsRec, 0x00AD371C>;
	using DurabilityQualityDB = Table<DurabilityQualityRec, 0x00AD3740>;
	using EmotesDB = Table<EmotesRec, 0x00AD3764>;
	using EmotesTextDataDB = Table<EmotesTextDataRec, 0x00AD3788>;
	using EmotesTextDB = Table<EmotesTextRec, 0x00AD37D0>;
	using EmotesTextSoundDB = Table<EmotesTextSoundRec, 0x00AD37AC>;
	using EnvironmentalDamageDB = Table<EnvironmentalDamageRec, 0x00AD37F4>;
	using ExhaustionDB = Table<ExhaustionRec, 0x00AD3818>;
	using FactionDB = Table<FactionRec, 0x00AD3860>;
	using FactionGroupDB = Table<FactionGroupRec, 0x00AD383C>;
	using FactionTemplateDB = Table<FactionTemplateRec, 0x00AD3884>;
	using FileDataDB = Table<FileDataRec, 0x00AD38A8>;
	using FootprintTexturesDB = Table<FootprintTexturesRec, 0x00AD38CC>;
	using FootstepTerrainLookupDB = Table<FootstepTerrainLookupRec, 0x00AD38F0>;
	using GameObjectArtKitDB = Table<GameObjectArtKitRec, 0x00AD3914>;
	using GameObjectDisplayInfoDB = Table<GameObjectDisplayInfoRec, 0x00AD3938>;
	using GameTablesDB = Table<GameTablesRec, 0x00AD395C>;
	using GameTipsDB = Table<GameTipsRec, 0x00AD3980>;
	using GemPropertiesDB = Table<GemPropertiesRec, 0x00AD39A4>;
	using GlyphPropertiesDB = Table<GlyphPropertiesRec, 0x00AD39C8>;
	using GlyphSlotDB = Table<GlyphSlotRec, 0x00AD39EC>;
	using GMSurveyAnswersDB = Table<GMSurveyAnswersRec, 0x00AD3A10>;
	using GMSurveyCurrentSurveyDB = Table<GMSurveyCurrentSurveyRec, 0x00AD3A34>;
	using GMSurveyQuestionsDB = Table<GMSurveyQuestionsRec, 0x00AD3A58>;
	using GMSurveySurveysDB = Table<GMSurveySurveysRec, 0x00AD3A7C>;
	using GMTicketCategoryDB = Table<GMTicketCategoryRec, 0x00AD3AA0>;
	using GroundEffectDoodadDB = Table<GroundEffectDoodadRec, 0x00AD3AC4>;
	using GroundEffectTextureDB = Table<GroundEffectTextureRec, 0x00AD3AE8>;
	using GtBarberShopCostBaseDB = Table<gtBarberShopCostBaseRec, 0x00AD3B0C>;
	using GtChanceToMeleeCritBaseDB = Table<gtChanceToMeleeCritBaseRec, 0x00AD3B78>;
	using GtChanceToMeleeCritDB = Table<gtChanceToMeleeCritRec, 0x00AD3B54>;
	using GtChanceToSpellCritBaseDB = Table<gtChanceToSpellCritBaseRec, 0x00AD3BC0>;
	using GtChanceToSpellCritDB = Table<gtChanceToSpellCritRec, 0x00AD3B9C>;
	using GtCombatRatingsDB = Table<gtCombatRatingsRec, 0x00AD3B30>;
	using GtNPCManaCostScalerDB = Table<gtNPCManaCostScalerRec, 0x00AD3BE4>;
	using GtOCTClassCombatRatingScalarDB = Table<gtOCTClassCombatRatingScalarRec, 0x00AD3C08>;
	using GtOCTRegenHPDB = Table<gtOCTRegenHPRec, 0x00AD3C2C>;
	using GtOCTRegenMPDB = Table<gtOCTRegenMPRec, 0x00AD3C50>;
	using GtRegenHPPerSptDB = Table<gtRegenHPPerSptRec, 0x00AD3C74>;
	using GtRegenMPPerSptDB = Table<gtRegenMPPerSptRec, 0x00AD3C98>;
	using HelmetGeosetVisDataDB = Table<HelmetGeosetVisDataRec, 0x00AD3CBC>;
	using HolidayDescriptionsDB = Table<HolidayDescriptionsRec, 0x00AD3CE0>;
	using HolidayNamesDB = Table<HolidayNamesRec, 0x00AD3D04>;
	using HolidaysDB = Table<HolidaysRec, 0x00AD3D28>;
	using ItemBagFamilyDB = Table<ItemBagFamilyRec, 0x00AD3D70>;
	using ItemClassDB = Table<ItemClassRec, 0x00AD3D94>;
	using ItemCondExtCostsDB = Table<ItemCondExtCostsRec, 0x00AD3DB8>;
	using ItemDB = Table<ItemRec, 0x00AD3D4C>;
	using ItemDisplayInfoDB = Table<ItemDisplayInfoRec, 0x00AD3DDC, Storage::Packed>;
	using ItemExtendedCostDB = Table<ItemExtendedCostRec, 0x00AD3E00>;
	using ItemGroupSoundsDB = Table<ItemGroupSoundsRec, 0x00AD3E24>;
	using ItemLimitCategoryDB = Table<ItemLimitCategoryRec, 0x00AD3E48>;
	using ItemPetFoodDB = Table<ItemPetFoodRec, 0x00AD3E6C>;
	using ItemPurchaseGroupDB = Table<ItemPurchaseGroupRec, 0x00AD3E90>;
	using ItemRandomPropertiesDB = Table<ItemRandomPropertiesRec, 0x00AD3EB4>;
	using ItemRandomSuffixDB = Table<ItemRandomSuffixRec, 0x00AD3ED8>;
	using ItemSetDB = Table<ItemSetRec, 0x00AD3EFC>;
	using ItemSubClassDB = Table<ItemSubClassRec, 0x00AD3F44>;
	using ItemSubClassMaskDB = Table<ItemSubClassMaskRec, 0x00AD3F20>;
	using ItemVisualEffectsDB = Table<ItemVisualEffectsRec, 0x00AD3F68>;
	using ItemVisualsDB = Table<ItemVisualsRec, 0x00AD3F8C>;
	using LanguagesDB = Table<LanguagesRec, 0x00AD3FD4>;
	using LanguageWordsDB = Table<LanguageWordsRec, 0x00AD3FB0>;
	using LfgDungeonExpansionDB = Table<LFGDungeonExpansionRec, 0x00AD3FF8>;
	using LfgDungeonGroupDB = Table<LFGDungeonGroupRec, 0x00AD401C>;
	using LfgDungeonsDB = Table<LFGDungeonsRec, 0x00AD4040>;
	using LightDB = Table<LightRec, 0x00AF4A28>;
	using LightFloatBandDB = Table<LightFloatBandRec, 0x00AF49E0, Storage::Packed>;
	using LightIntBandDB = Table<LightIntBandRec, 0x00AF49BC, Storage::Packed>;
	using LightParamsDB = Table<LightParamsRec, 0x00AF4A04>;
	using LightSkyboxDB = Table<LightSkyboxRec, 0x00AF4998>;
	using LiquidMaterialDB = Table<LiquidMaterialRec, 0x00AD4088>;
	using LiquidTypeDB = Table<LiquidTypeRec, 0x00AD4064>;
	using LoadingScreensDB = Table<LoadingScreensRec, 0x00AD40AC>;
	using LoadingScreenTaxiSplinesDB = Table<LoadingScreenTaxiSplinesRec, 0x00AD40D0>;
	using LockDB = Table<LockRec, 0x00AD40F4>;
	using LockTypeDB = Table<LockTypeRec, 0x00AD4118>;
	using MailTemplateDB = Table<MailTemplateRec, 0x00AD413C>;
	using MapDB = Table<MapRec, 0x00AD4160>;
	using MapDifficultyDB = Table<MapDifficultyRec, 0x00AD4184>;
	using MaterialDB = Table<MaterialRec, 0x00AD41A8>;
	using MovieDB = Table<MovieRec, 0x00AD41CC>;
	using MovieFileDataDB = Table<MovieFileDataRec, 0x00AD41F0>;
	using MovieVariationDB = Table<MovieVariationRec, 0x00AD4214>;
	using NameGenDB = Table<NameGenRec, 0x00AD4238>;
	using NamesProfanityDB = Table<NamesProfanityRec, 0x00AD4280>;
	using NamesReservedDB = Table<NamesReservedRec, 0x00AD42A4>;
	using NPCSoundsDB = Table<NPCSoundsRec, 0x00AD425C>;
	using ObjectEffectDB = Table<ObjectEffectRec, 0x00AD5048>;
	using ObjectEffectGroupDB = Table<ObjectEffectGroupRec, 0x00AD506C>;
	using ObjectEffectModifierDB = Table<ObjectEffectModifierRec, 0x00AD5090>;
	using ObjectEffectPackageDB = Table<ObjectEffectPackageRec, 0x00AD50B4>;
	using ObjectEffectPackageElemDB = Table<ObjectEffectPackageElemRec, 0x00AD50D8>;
	using OverrideSpellDataDB = Table<OverrideSpellDataRec, 0x00AD42C8>;
	using PackageDB = Table<PackageRec, 0x00AD42EC>;
	using PageTextMaterialDB = Table<PageTextMaterialRec, 0x00AD4310>;
	using PaperDollItemFrameDB = Table<PaperDollItemFrameRec, 0x00AD4334>;
	using ParticleColorDB = Table<ParticleColorRec, 0x00AD4358>;
	using PetPersonalityDB = Table<PetPersonalityRec, 0x00AD437C>;
	using PowerDisplayDB = Table<PowerDisplayRec, 0x00AD43A0>;
	using PvpDifficultyDB = Table<PVPDifficultyRec, 0x00AD43C4>;
	using QuestFactionRewardDB = Table<QuestFactionRewardRec, 0x00AD43E8>;
	using QuestInfoDB = Table<QuestInfoRec, 0x00AD440C>;
	using QuestSortDB = Table<QuestSortRec, 0x00AD4430>;
	using QuestXPDB = Table<QuestXPRec, 0x00AD4454>;
	using RandPropPointsDB = Table<RandPropPointsRec, 0x00AD449C>;
	using ResistancesDB = Table<ResistancesRec, 0x00AD4478>;
	using ScalingStatDistributionDB = Table<ScalingStatDistributionRec, 0x00AD44C0>;
	using ScalingStatValuesDB = Table<ScalingStatValuesRec, 0x00AD44E4>;
	using ScreenEffectDB = Table<ScreenEffectRec, 0x00AD4508>;
	using ServerMessagesDB = Table<ServerMessagesRec, 0x00AD452C>;
	using SheatheSoundLookupsDB = Table<SheatheSoundLookupsRec, 0x00AD4550>;
	using SkillCostsDataDB = Table<SkillCostsDataRec, 0x00AD4574>;
	using SkillLineAbilityDB = Table<SkillLineAbilityRec, 0x00AD4598>;
	using SkillLineCategoryDB = Table<SkillLineCategoryRec, 0x00AD45BC>;
	using SkillLineDB = Table<SkillLineRec, 0x00AD45E0>;
	using SkillRaceClassInfoDB = Table<SkillRaceClassInfoRec, 0x00AD4604>;
	using SkillTiersDB = Table<SkillTiersRec, 0x00AD4628>;
	using SoundAmbienceDB = Table<SoundAmbienceRec, 0x00AD464C>;
	using SoundEmittersDB = Table<SoundEmittersRec, 0x00AD4694>;
	using SoundEntriesAdvancedDB = Table<SoundEntriesAdvancedRec, 0x00AD5024>;
	using SoundEntriesDB = Table<SoundEntriesRec, 0x00AD4670>;
	using SoundFilterDB = Table<SoundFilterRec, 0x00AD50FC>;
	using SoundFilterElemDB = Table<SoundFilterElemRec, 0x00AD5120>;
	using SoundProviderPreferencesDB = Table<SoundProviderPreferencesRec, 0x00AD46B8>;
	using SoundSamplePreferencesDB = Table<SoundSamplePreferencesRec, 0x00AD46DC>;
	using SoundWaterTypeDB = Table<SoundWaterTypeRec, 0x00AD4700>;
	using SpamMessagesDB = Table<SpamMessagesRec, 0x00AD4724>;
	using SpellCastTimesDB = Table<SpellCastTimesRec, 0x00AD4748>;
	using SpellCategoryDB = Table<SpellCategoryRec, 0x00AD476C>;
	using SpellChainEffectsDB = Table<SpellChainEffectsRec, 0x00AD4790>;
	using SpellDB = Table<SpellRec, 0x00AD49D0, Storage::Packed>;
	using SpellDescriptionVariablesDB = Table<SpellDescriptionVariablesRec, 0x00AD47B4>;
	using SpellDifficultyDB = Table<SpellDifficultyRec, 0x00AD47D8>;
	using SpellDispelTypeDB = Table<SpellDispelTypeRec, 0x00AD47FC>;
	using SpellDurationDB = Table<SpellDurationRec, 0x00AD4820>;
	using SpellEffectCameraShakesDB = Table<SpellEffectCameraShakesRec, 0x00AD4844>;
	using SpellFocusObjectDB = Table<SpellFocusObjectRec, 0x00AD4868>;
	using SpellIconDB = Table<SpellIconRec, 0x00AD488C>;
	using SpellItemEnchantmentConditionDB = Table<SpellItemEnchantmentConditionRec, 0x00AD48D4>;
	using SpellItemEnchantmentDB = Table<SpellItemEnchantmentRec, 0x00AD48B0>;
	using SpellMechanicDB = Table<SpellMechanicRec, 0x00AD48F8>;
	using SpellMissileDB = Table<SpellMissileRec, 0x00AD491C>;
	using SpellMissileMotionDB = Table<SpellMissileMotionRec, 0x00AD4940>;
	using SpellRadiusDB = Table<SpellRadiusRec, 0x00AD4964>;
	using SpellRangeDB = Table<SpellRangeRec, 0x00AD4988>;
	using SpellRuneCostDB = Table<SpellRuneCostRec, 0x00AD49AC>;
	using SpellShapeshiftFormDB = Table<SpellShapeshiftFormRec, 0x00AD49F4>;
	using SpellVisualDB = Table<SpellVisualRec, 0x00AD4AA8>;
	using SpellVisualEffectNameDB = Table<SpellVisualEffectNameRec, 0x00AD4A18>;
	using SpellVisualKitAreaModelDB = Table<SpellVisualKitAreaModelRec, 0x00AD4A60>;
	using SpellVisualKitDB = Table<SpellVisualKitRec, 0x00AD4A3C>;
	using SpellVisualKitModelAttachDB = Table<SpellVisualKitModelAttachRec, 0x00AD4A84>;
	using StableSlotPricesDB = Table<StableSlotPricesRec, 0x00AD4ACC>;
	using Startup_StringsDB = Table<Startup_StringsRec, 0x00AB6350>;
	using StationeryDB = Table<StationeryRec, 0x00AD4AF0>;
	using StringLookupsDB = Table<StringLookupsRec, 0x00AD4B14>;
	using SummonPropertiesDB = Table<SummonPropertiesRec, 0x00AD4B38>;
	using TalentDB = Table<TalentRec, 0x00AD4B5C>;
	using TalentTabDB = Table<TalentTabRec, 0x00AD4B80>;
	using TaxiNodesDB = Table<TaxiNodesRec, 0x00AD4BA4>;
	using TaxiPathDB = Table<TaxiPathRec, 0x00AD4BEC>;
	using TaxiPathNodeDB = Table<TaxiPathNodeRec, 0x00AD4BC8>;
	using TeamContributionPointsDB = Table<TeamContributionPointsRec, 0x00AD4C10>;
	using TerrainTypeDB = Table<TerrainTypeRec, 0x00AD4C34>;
	using TerrainTypeSoundsDB = Table<TerrainTypeSoundsRec, 0x00AD4C58>;
	using TotemCategoryDB = Table<TotemCategoryRec, 0x00AD4C7C>;
	using TransportAnimationDB = Table<TransportAnimationRec, 0x00AD4CA0>;
	using TransportPhysicsDB = Table<TransportPhysicsRec, 0x00AD4CC4>;
	using TransportRotationDB = Table<TransportRotationRec, 0x00AD4CE8>;
	using UISoundLookupsDB = Table<UISoundLookupsRec, 0x00AD4D0C>;
	using UnitBloodDB = Table<UnitBloodRec, 0x00AD4D54>;
	using UnitBloodLevelsDB = Table<UnitBloodLevelsRec, 0x00AD4D30>;
	using VehicleDB = Table<VehicleRec, 0x00AD4D78>;
	using VehicleSeatDB = Table<VehicleSeatRec, 0x00AD4D9C>;
	using VehicleUIIndicatorDB = Table<VehicleUIIndicatorRec, 0x00AD4DC0>;
	using VehicleUIIndSeatDB = Table<VehicleUIIndSeatRec, 0x00AD4DE4>;
	using VideoHardwareDB = Table<VideoHardwareRec, 0x00ADBF88>;
	using VocalUISoundsDB = Table<VocalUISoundsRec, 0x00AD4E08>;
	using WeaponImpactSoundsDB = Table<WeaponImpactSoundsRec, 0x00AD4E50>;
	using WeaponSwingSounds2DB = Table<WeaponSwingSounds2Rec, 0x00AD4E74>;
	using WeatherDB = Table<WeatherRec, 0x00AD4E98>;
	using WMOAreaTableDB = Table<WMOAreaTableRec, 0x00AD4E2C>;
	using WorldChunkSoundsDB = Table<WorldChunkSoundsRec, 0x00AD5000>;
	using WorldMapAreaDB = Table<WorldMapAreaRec, 0x00AD4EBC>;
	using WorldMapContinentDB = Table<WorldMapContinentRec, 0x00AD4EE0>;
	using WorldMapOverlayDB = Table<WorldMapOverlayRec, 0x00AD4F04>;
	using WorldMapTransformsDB = Table<WorldMapTransformsRec, 0x00AD4F28>;
	using WorldSafeLocsDB = Table<WorldSafeLocsRec, 0x00AD4F4C>;
	using WorldStateUIDB = Table<WorldStateUIRec, 0x00AD4F70>;
	using WorldStateZoneSoundsDB = Table<WorldStateZoneSoundsRec, 0x00AD4FDC>;
	using ZoneIntroMusicTableDB = Table<ZoneIntroMusicTableRec, 0x00AD4F94>;
	using ZoneMusicDB = Table<ZoneMusicRec, 0x00AD4FB8>;
}
