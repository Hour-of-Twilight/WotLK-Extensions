#include "SharedDefines.h"
static constexpr uint32 ADDITIONAL_STATS = 24;
static constexpr uint32 DEFAULT_MOD_COUNT = 49;
static constexpr uint32 DEFAULT_MOD_VAL_COUNT = 37;
static constexpr uint32 MOD_COUNT = DEFAULT_MOD_COUNT + ADDITIONAL_STATS;
static constexpr uint32 MOD_VAL_COUNT = DEFAULT_MOD_VAL_COUNT + ADDITIONAL_STATS;
static constexpr const char* customItemModStrings[ADDITIONAL_STATS] = {
	"ITEM_MOD_MAGIC_FIND",
	"ITEM_MOD_LEECH",
	"ITEM_MOD_MANA_LEECH",
	"ITEM_MOD_CRIT_DAMAGE",
	"ITEM_MOD_CRIT_HEALING",
	"ITEM_MOD_FOCUS",
	"ITEM_MOD_PCT_FOCUS",
	"ITEM_MOD_PCT_ATK_POWER",
	"ITEM_MOD_PCT_SPELL_DMG",
	"ITEM_MOD_PCT_HEALING",
	"ITEM_MOD_PCT_STR",
	"ITEM_MOD_PCT_AGI",
	"ITEM_MOD_PCT_STAM",
	"ITEM_MOD_PCT_INT",
	"ITEM_MOD_PCT_SPI",
	"ITEM_MOD_PCT_ALL_STAT",
	"ITEM_MOD_PCT_RED_PHYS",
	"ITEM_MOD_PCT_RED_MAGIC",
	"ITEM_MOD_WEP_DMG_FLAT",
	"ITEM_MOD_WEP_DMG_PCT",
	"ITEM_MOD_ARMOR_PCT",
	"ITEM_MOD_ADDITONAL_SOCKET",
	"ITEM_MOD_MOVEMENT_SPEED",
	"ITEM_MOD_POTION_EFFECT"
};
class Misc
{
public:
	static void ApplyPatches();

	static void SetYearOffsetMultiplier();
	static void ToggleWireframeMode();
	static void ToggleDisplayNormals();
	static void ToggleTerrainCulling();
	static void ToggleGroundEffects();
	static void ToggleTerrain();
	static void ToggleLiquids();
	static void ToggleM2();
	static void ToggleWMO();

private:
	// defaults: 49 and 37 - both need to be increased by number of mods you add
	// itemModTableVal is capped on 127 until I rewrite some parts of the code - if I do that is
	// so and so itemModTable is capped to 139 I think :P
	static inline uint32_t itemModTable[MOD_COUNT] = { 0 };
	static inline uint32_t itemModTableVal[MOD_VAL_COUNT] = { 0 };

	static inline uint32_t yearOffsetMult = 0;

	static char* GetTimeString(WoWTime* a1, char* a2, uint32_t a3);
	static void PackTimeDataToDword(uint32_t* packedTime, int32_t minute, int32_t hour, int32_t weekDay, int32_t monthDay, int32_t month, int32_t year, int32_t flags);
	static void PackWoWTimeToDword(uint32_t* dword, WoWTime* time);
	static void UnpackWoWTime(uint32_t packedTime, int32_t* minute, int32_t* hour, int32_t* weekDay, int32_t* monthDay, int32_t* month, int32_t* year, int32_t* flags);
};
