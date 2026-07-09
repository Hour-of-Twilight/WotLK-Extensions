#pragma once
#include <Defines.h>
#include <ClientData/Enums.h>
#include <ClientData/ObjectModel.h>
#include <ClientData/DBCRows.h>
#include <ClientData/System.h>
#include <ClientData/Spell.h>
#include <ClientData/WorldFrame.h>

using ClientData::RCString;
using ClientData::CVar;
using ClientData::CVarCallback;
using ClientData::WoWClientDB;
using ClientData::WoWTime;
using ClientData::ZoneLightData;
using ClientData::TerrainClickEvent;
using ClientData::AuraData;
using ClientData::PendingSpellCastData;
using ClientData::PendingSpellCast;
using ClientData::WorldHitTest;
using ClientData::CSimpleTop;
using ClientData::CGWorldFrame;

struct CDataStore_vTable
{
	uint32_t padding0x00[6];
	void* IsRead;
	uint32_t padding0x1C[3];
};

struct CDataStore
{
	CDataStore_vTable* vTable;
	int32_t* m_buffer;
	int32_t m_base;
	int32_t m_alloc;
	int32_t m_size;
	int32_t m_read;
};

struct CustomNetClient
{
	void* handler[NUM_CUSTOM_MSG_TYPES - SMSG_UPDATE_CUSTOM_COMBAT_RATING];
	void* handlerParam[NUM_CUSTOM_MSG_TYPES - SMSG_UPDATE_CUSTOM_COMBAT_RATING];
};

struct SpellCastNode
{
	SpellCastNode* next;   // 0x00
	SpellCastNode* prev;   // 0x04
	uint8_t padding[0x18]; // 0x08
	uint32_t spellId;      // 0x20
};

struct ItemRecord
{
	uint32 itemID;                   // +0x00
	uint32 itemClass;                // +0x04
	uint32 itemSubClass;             // +0x08
	int32 sound_override_subclassid; // +0x0C
	int32 materialID;                // +0x10
	uint32 itemDisplayInfo;          // +0x14
	uint32 inventorySlotID;          // +0x18
	uint32 sheathID;                 // +0x1C
};

struct ItemCache
{
	uint32 Id;                   // +0x00
	int Class;                   // +0x04
	int SubClass;                // +0x08
	int UnkInt;                  // +0x0C
	uint32 DisplayId;            // +0x10
	int Quality;                 // +0x14
	int FlagsAndFaction[2];      // +0x18
	int BuyPrice;                // +0x20
	int SellPrice;               // +0x24
	int InvType;                 // +0x28
	int AllowClass;              // +0x2C
	int AllowRace;               // +0x30
	int ItemLvl;                 // +0x34
	int ReqLvl;                  // +0x38
	int ReqSkill;                // +0x3C
	int ReqSkillRank;            // +0x40
	int ReqSpell;                // +0x44
	int ReqHonor;                // +0x48
	int ReqCityRank;             // +0x4C
	int ReqRepFaction;           // +0x50
	int ReqRepRank;              // +0x54
	int MaxCount;                // +0x58
	int Stackable;               // +0x5C
	int ContainerSlots;          // +0x60
	int StatsCount;              // +0x64
	int Stats[20];               // +0x68
	int ScalingStatDistribution; // +0xB8
	int ScalingStatValue;        // +0xBC
	int SSDDmg1[2];              // +0xC0
	int SSDDmg2[2];              // +0xC8
	int SSDDmgType[2];           // +0xD0
	int Resistance[7];           // +0xD8
	int Delay;                   // +0xF4
	int AmmoType;                // +0xF8
	int RangedModRange;          // +0xFC
	int32 SpellId[5];            // +0x100
	int SpellTrigger[5];         // +0x114
	int SpellCharges[5];         // +0x128
	int SpellCooldown[5];        // +0x13C
	int SpellCategory[5];        // +0x150
	int SpellCatCooldown[5];     // +0x164
	int Bonding;                 // +0x178
	int Description;             // +0x17C
	int PageTextId;              // +0x180
	int LanguageId;              // +0x184
	int PageMaterial;            // +0x188
	int StartQuest;              // +0x18C
	int LockId;                  // +0x190
	int Material;                // +0x194
	int Sheath;                  // +0x198
	int RandomProperty;          // +0x19C
	int RandomSuffix;            // +0x1A0
	int Block;                   // +0x1A4
	int ItemSetId;               // +0x1A8
	int MaxDurability;           // +0x1AC
	int Area;                    // +0x1B0
	int Map;                     // +0x1B4
	int BagFamily;               // +0x1B8
	int TotemCategory;           // +0x1BC
	int SocketColor[3];          // +0x1C0
	int SocketItem[3];           // +0x1CC
	int SocketBonus;             // +0x1D8
	int GemProperties;           // +0x1DC
	int ReqDisenchantSkill;      // +0x1E0
	int ArmorDmgMod;             // +0x1E4
	int Duration;                // +0x1E8
	int ItemLimitCat;            // +0x1EC
	int Holiday;                 // +0x1F0
	char Name[1600];             // +0x1F4
};

struct WowClientDB_Base
{
	void* v_table;    // +0x00
	int m_loaded;     // +0x04
	int m_numRecords; // +0x08
	int m_maxID;      // +0x0C
	int m_minID;      // +0x10
	void* m_strings;  // +0x14
};

struct IDatabase_ItemRec
{
	void* v_table;              // +0x00
	ItemRecord* m_records;      // +0x04
	ItemRecord** m_recordsById; // +0x08
};

struct WowClientDB_ItemRec
{
	WowClientDB_Base b_base_01;  // +0x00 (size 0x18)
	IDatabase_ItemRec b_base_02; // +0x18 (size 0x0C)
};

struct IDatabase_ItemDisplayInfoRec
{
	void* v_table;                      // +0x00 (+0x18 global)
	ItemDisplayInfoRec* m_records;      // +0x04 (+0x1C global)
	void* m_unk;                        // +0x08 (+0x20 global)
	ItemDisplayInfoRec** m_recordsById; // +0x0C (+0x24 global)
};

struct WowClientDB_ItemDisplayInfoRec
{
	WowClientDB_Base b_base_01;             // +0x00 (size 0x18)
	IDatabase_ItemDisplayInfoRec b_base_02; // +0x18 (size 0x10)
};
