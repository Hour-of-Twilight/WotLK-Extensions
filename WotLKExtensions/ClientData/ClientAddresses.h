#pragma once
#include <Macros.h>
#include <ClientData/GameStructs.h>
#include <ClientData/DBCRows.h>

// These numbers are only valid when ingame.
constexpr const int HOT_ITEM_CACHE_UPDATE_ID = 722;
constexpr const int HOT_STAT_UPDATE_ID = 723;
constexpr const int HOT_BAD_MPQ_NUM = 724;
constexpr const int HOT_BAD_MPQ_FILE_INFO = 725;
constexpr const int HOT_CHAR_SELECT_UPDATE = 726;
constexpr const int HOT_GOB_SELECTED = 727;

CLIENT_ADDRESS(WoWClientDB, g_SpellDB, 0x00AD49D0)
CLIENT_ADDRESS(WoWClientDB, g_SpellCategoryDB, 0x00AD477C)
CLIENT_ADDRESS(WoWClientDB, g_CreatureFamilyDB, 0x00AD34EC)
CLIENT_ADDRESS(WoWClientDB, g_ItemPetFoodDB, 0x00AD3E74)
CLIENT_ADDRESS(WoWClientDB, g_SkillLineAbilityDB, 0x00AD45A0)
CLIENT_ADDRESS(WoWClientDB, g_SpellRuneCostDB, 0x00AD49C4)

CLIENT_ADDRESS(WowClientDB_ItemDisplayInfoRec, g_itemDisplayInfoDB, 0x00AD3DDC)

CLIENT_ADDRESS(int, dword_AD2D30, 0x00AD2D30)
CLIENT_ADDRESS(uint32, unk_C0100F, 0xC0100F)
CLIENT_ADDRESS(void*, dword_AF5254, 0x00AF5254)
CLIENT_ADDRESS(void*, spellCast, 0x00D3F4E4)
CLIENT_ADDRESS(WowClientDB_ItemRec, g_itemDB, 0x00AD3D4C)
CLIENT_ADDRESS(void*, WDB_CACHE_ITEM, 0x00C5D828)
CLIENT_ADDRESS(uint64_t, g_comboPointTarget, 0x00BD08A8)
CLIENT_ADDRESS(uint8_t, g_comboPointCount, 0x00BD084D)
CLIENT_ADDRESS(uint64_t, CGGameUI__m_lockedTarget, 0x00BD07B0)
CLIENT_ADDRESS(uint32_t, g_whoDisplayCount, 0xC7BFE0)
CLIENT_ADDRESS(uint32_t, g_whoMatchCount, 0xC7BFE4)
CLIENT_ADDRESS(uint32_t, g_whoFilterActive, 0xC7BFEC)
CLIENT_ADDRESS(char, g_clientWhoNames, 0xC79FD8)     // char[50][0x30]
CLIENT_ADDRESS(char, g_clientWhoGuilds, 0xC7A008)    // char[50][0x60]
CLIENT_ADDRESS(uint32_t, g_clientWhoLevel, 0xC7A068) // uint32[50] stride 0xA4
CLIENT_ADDRESS(uint32_t, g_clientWhoRace, 0xC7A06C)
CLIENT_ADDRESS(uint32_t, g_clientWhoClass, 0xC7A070)
CLIENT_ADDRESS(uint32_t, g_clientWhoSex, 0xC7A074)
CLIENT_ADDRESS(uint32_t, g_clientWhoZone, 0xC7A078)
CLIENT_ADDRESS(WoWClientDB, g_areaTableDB, 0x00AD3134)
CLIENT_ADDRESS(WoWClientDB, g_chrClassesDB, 0x00AD3404)
CLIENT_ADDRESS(WoWClientDB, g_chrRacesDB, 0x00AD3428)
CLIENT_ADDRESS(void*, g_friendList, 0x00C79F98)
CLIENT_ADDRESS(void, WDB_CACHE_GUILD, 0x00C5D9C0)
CLIENT_ADDRESS(uint32_t, g_guildRosterCount, 0x00C22AB0)
CLIENT_ADDRESS(uint32_t, g_guildRosterOnline, 0x00C22AB4)
CLIENT_ADDRESS(void*, g_guildRosterObject, 0x00C22AD8)
CLIENT_ADDRESS(uint32_t, g_guildRosterCapacity, 0x00C22ADC)
CLIENT_ADDRESS(void*, g_guildRosterArray, 0x00C22AE0)
CLIENT_ADDRESS(bool, g_isDBCCompressed, 0x00C5DEA0)
CLIENT_ADDRESS(void, CDataStore__v_table, 0x009E0E24)
CLIENT_ADDRESS(void, g_gemPropertiesDB, 0x00AD39A4)
CLIENT_ADDRESS(uint32_t, g_socketInfoGuidLow, 0x00C20FE0)
CLIENT_ADDRESS(uint32_t, g_socketInfoGuidHigh, 0x00C20FE4)
CLIENT_ADDRESS(uint32_t, g_socketInfoPtr, 0x00C20FE8)
CLIENT_ADDRESS(void*, dword_B6B1A0, 0x00B6B1A0)

inline uint8_t* GetFriendListBase()
{
	return *reinterpret_cast<uint8_t**>(0x00C79F98);
}

constexpr uint32 AFK_TIMER_ADDR = 0x00B499A4;
constexpr uint32 OS_GET_TIME_ADDR = 0x0086AE20;
constexpr uint32 AFK_CHECK_SUB = 0x0052B251;
constexpr uint32 AFK_5MIN_CHECK = 0x0052B25F;
constexpr uint32 AFK_30MIN_CHECK = 0x0052B26A;
