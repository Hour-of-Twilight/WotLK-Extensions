#pragma once
#include <Defines.h>
#include <ClientData/DBCRowTypes.h>
#include <ClientData/Spell.h>

using ClientData::ChrClassesRow;
using ClientData::gtCombatRatingsRow;
using ClientData::gtOCTClassCombatRatingScalarRow;
using ClientData::MapRow;
using ClientData::PowerDisplayRow;
using ClientData::SkillLineAbilityRow;
using ClientData::SkillLineRow;
using ClientData::SpellRow;

struct CreatureStats
{
	char padding0x00[0x48];
	int health; // +0x48
	char padding0x4C[0xB4];
	float minDamage; // +0x100
	float maxDamage; // +0x104
	char padding0x108[0x6C];
	int resistances[7]; // +0x174
};

struct CreatureType
{
	char padding0x00[0x0C];
	int flags; // +0x0C
	char padding0x10[0x04];
	int creatureTypeFlags; // +0x14
};

struct CreatureFamilyRow
{
	int id;
	char padding0x04[0x18];
	int petFoodMask;   // +0x1C
	int skillLines[2]; // +0x20
};

struct ItemPetFoodRow
{
	int petFoodType;
	const char* foodName;
};

struct ItemDisplayInfoRec
{
	int32_t m_ID;                   // +0x00
	const char* m_modelName[2];     // +0x04
	const char* m_modelTexture[2];  // +0x0C
	const char* m_inventoryIcon[2]; // +0x14
	int32_t m_geosetGroup[3];       // +0x1C
	int32_t m_flags;                // +0x28
	int32_t m_spellVisualID;        // +0x2C
	int32_t m_groupSoundIndex;      // +0x30
	int32_t m_helmetGeosetVisID[2]; // +0x34
	const char* m_texture[8];       // +0x3C
	int32_t m_itemVisual;           // +0x5C
	int32_t m_particleColorID;      // +0x60
}; // sizeof = 0x64
