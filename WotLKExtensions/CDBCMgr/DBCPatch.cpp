#include "DBCPatch.h"

#include <ClientData/ClientFunctions.h>
#include <SharedDefines.h>
#include <ClientDetours.h>

#include <CDBCMgr.h>
#include <Logger.h>

#include <cstring>
#include <cstdint>
#include <algorithm>
#include <string>
#include <vector>

using ClientData::WoWClientDB;

std::vector<DBCPatch::PatchRange>& DBCPatch::PatchedRanges()
{
	static std::vector<PatchRange> v;
	return v;
}

void DBCPatch::AddPatchedRange(const void* begin, const void* end)
{
	auto& v = PatchedRanges();
	PatchRange r{ reinterpret_cast<uintptr_t>(begin), reinterpret_cast<uintptr_t>(end) };
	auto it = std::lower_bound(v.begin(), v.end(), r.begin,
	    [](const PatchRange& e, uintptr_t val)
	{
		return e.begin < val;
	});
	v.insert(it, r);
}

bool DBCPatch::IsPatchedRow(const void* p)
{
	auto& v = PatchedRanges();
	uintptr_t a = reinterpret_cast<uintptr_t>(p);
	auto it = std::upper_bound(v.begin(), v.end(), a,
	    [](uintptr_t val, const PatchRange& e)
	{
		return val < e.begin;
	});
	if (it == v.begin())
		return false;
	--it;
	return a >= it->begin && a < it->end;
}

// Hand our patched rows back uncompressed; RLE-decompress everything else.
CLIENT_DETOUR(DecompressRow, 0x004CFBB0, __cdecl, void, (void* src, int size, void* out))
{
	if (DBCPatch::IsPatchedRow(src))
		std::memcpy(out, src, static_cast<size_t>(size));
	else
		DecompressRow(src, size, out);
}

struct DBCEntry
{
	const char* name;
	uint32_t base;
};

constexpr DBCEntry kStorages[] = {
	{ "achievement", 0x00AD305Cu },
	{ "achievement_Category", 0x00AD30A4u },
	{ "achievement_Criteria", 0x00AD3080u },
	{ "animationData", 0x00AD30C8u },
	{ "areaGroup", 0x00AD30ECu },
	{ "areaPOI", 0x00AD3110u },
	{ "areaTable", 0x00AD3134u },
	{ "areaTrigger", 0x00AD3158u },
	{ "attackAnimKits", 0x00AD317Cu },
	{ "attackAnimTypes", 0x00AD31A0u },
	{ "auctionHouse", 0x00AD31C4u },
	{ "bankBagSlotPrices", 0x00AD31E8u },
	{ "bannedAddOns", 0x00AD320Cu },
	{ "barberShopStyle", 0x00AD3230u },
	{ "battlemasterList", 0x00AD3254u },
	{ "cameraShakes", 0x00AD3278u },
	{ "cfg_Categories", 0x00AD329Cu },
	{ "cfg_Configs", 0x00AD32C0u },
	{ "charBaseInfo", 0x00AD32E4u },
	{ "charHairGeosets", 0x00AD3308u },
	{ "charSections", 0x00AD332Cu },
	{ "charStartOutfit", 0x00AD3350u },
	{ "charTitles", 0x00AD3374u },
	{ "characterFacialHairStyles", 0x00AD3398u },
	{ "chatChannels", 0x00AD33BCu },
	{ "chatProfanity", 0x00AD33E0u },
	{ "chrClasses", 0x00AD3404u },
	{ "chrRaces", 0x00AD3428u },
	{ "cinematicCamera", 0x00AD344Cu },
	{ "cinematicSequences", 0x00AD3470u },
	{ "creatureDisplayInfo", 0x00AD34B8u },
	{ "creatureDisplayInfoExtra", 0x00AD3494u },
	{ "creatureFamily", 0x00AD34DCu },
	{ "creatureModelData", 0x00AD3500u },
	{ "creatureMovementInfo", 0x00AD3524u },
	{ "creatureSoundData", 0x00AD3548u },
	{ "creatureSpellData", 0x00AD356Cu },
	{ "creatureType", 0x00AD3590u },
	{ "currencyCategory", 0x00AD35D8u },
	{ "currencyTypes", 0x00AD35B4u },
	{ "danceMoves", 0x00AD35FCu },
	{ "deathThudLookups", 0x00AD3620u },
	{ "destructibleModelData", 0x00AD368Cu },
	{ "dungeonEncounter", 0x00AD36B0u },
	{ "dungeonMapChunk", 0x00AD36F8u },
	{ "dungeonMap", 0x00AD36D4u },
	{ "durabilityCosts", 0x00AD371Cu },
	{ "durabilityQuality", 0x00AD3740u },
	{ "emotes", 0x00AD3764u },
	{ "emotesText", 0x00AD37D0u },
	{ "emotesTextData", 0x00AD3788u },
	{ "emotesTextSound", 0x00AD37ACu },
	{ "environmentalDamage", 0x00AD37F4u },
	{ "exhaustion", 0x00AD3818u },
	{ "faction", 0x00AD3860u },
	{ "factionGroup", 0x00AD383Cu },
	{ "factionTemplate", 0x00AD3884u },
	{ "fileData", 0x00AD38A8u },
	{ "footprintTextures", 0x00AD38CCu },
	{ "footstepTerrainLookup", 0x00AD38F0u },
	{ "gMSurveyAnswers", 0x00AD3A10u },
	{ "gMSurveyCurrentSurvey", 0x00AD3A34u },
	{ "gMSurveyQuestions", 0x00AD3A58u },
	{ "gMSurveySurveys", 0x00AD3A7Cu },
	{ "gMTicketCategory", 0x00AD3AA0u },
	{ "gameObjectArtKit", 0x00AD3914u },
	{ "gameObjectDisplayInfo", 0x00AD3938u },
	{ "gameTables", 0x00AD395Cu },
	{ "gameTips", 0x00AD3980u },
	{ "gemProperties", 0x00AD39A4u },
	{ "glyphProperties", 0x00AD39C8u },
	{ "glyphSlot", 0x00AD39ECu },
	{ "groundEffectDoodad", 0x00AD3AC4u },
	{ "groundEffectTexture", 0x00AD3AE8u },
	{ "gtBarberShopCostBase", 0x00AD3B0Cu },
	{ "gtChanceToMeleeCritBase", 0x00AD3B78u },
	{ "gtChanceToMeleeCrit", 0x00AD3B54u },
	{ "gtChanceToSpellCritBase", 0x00AD3BC0u },
	{ "gtChanceToSpellCrit", 0x00AD3B9Cu },
	{ "gtCombatRatings", 0x00AD3B30u },
	{ "gtNPCManaCostScaler", 0x00AD3BE4u },
	{ "gtOCTClassCombatRatingScalar", 0x00AD3C08u },
	{ "gtOCTRegenHP", 0x00AD3C2Cu },
	{ "gtOCTRegenMP", 0x00AD3C50u },
	{ "gtRegenHPPerSpt", 0x00AD3C74u },
	{ "gtRegenMPPerSpt", 0x00AD3C98u },
	{ "helmetGeosetVisData", 0x00AD3CBCu },
	{ "holidayDescriptions", 0x00AD3CE0u },
	{ "holidayNames", 0x00AD3D04u },
	{ "holidays", 0x00AD3D28u },
	{ "itemBagFamily", 0x00AD3D70u },
	{ "itemClass", 0x00AD3D94u },
	{ "itemCondExtCosts", 0x00AD3DB8u },
	{ "item", 0x00AD3D4Cu },
	{ "itemDisplayInfo", 0x00AD3DDCu },
	{ "itemExtendedCost", 0x00AD3E00u },
	{ "itemGroupSounds", 0x00AD3E24u },
	{ "itemLimitCategory", 0x00AD3E48u },
	{ "itemPetFood", 0x00AD3E6Cu },
	{ "itemPurchaseGroup", 0x00AD3E90u },
	{ "itemRandomProperties", 0x00AD3EB4u },
	{ "itemRandomSuffix", 0x00AD3ED8u },
	{ "itemSet", 0x00AD3EFCu },
	{ "itemSubClass", 0x00AD3F44u },
	{ "itemSubClassMask", 0x00AD3F20u },
	{ "itemVisualEffects", 0x00AD3F68u },
	{ "itemVisuals", 0x00AD3F8Cu },
	{ "languageWords", 0x00AD3FB0u },
	{ "languages", 0x00AD3FD4u },
	{ "lfgDungeonExpansion", 0x00AD3FF8u },
	{ "lfgDungeonGroup", 0x00AD401Cu },
	{ "lfgDungeons", 0x00AD4040u },
	{ "light", 0x00AF4A28u },
	{ "lightFloatBand", 0x00AF49E0u },
	{ "lightIntBand", 0x00AF49BCu },
	{ "lightParams", 0x00AF4A04u },
	{ "lightSkybox", 0x00AF4998u },
	{ "liquidMaterial", 0x00AD4088u },
	{ "liquidType", 0x00AD4064u },
	{ "loadingScreenTaxiSplines", 0x00AD40D0u },
	{ "loadingScreens", 0x00AD40ACu },
	{ "lock", 0x00AD40F4u },
	{ "lockType", 0x00AD4118u },
	{ "mailTemplate", 0x00AD413Cu },
	{ "map", 0x00AD4160u },
	{ "mapDifficulty", 0x00AD4184u },
	{ "material", 0x00AD41A8u },
	{ "movie", 0x00AD41CCu },
	{ "movieFileData", 0x00AD41F0u },
	{ "movieVariation", 0x00AD4214u },
	{ "nPCSounds", 0x00AD425Cu },
	{ "nameGen", 0x00AD4238u },
	{ "namesProfanity", 0x00AD4280u },
	{ "namesReserved", 0x00AD42A4u },
	{ "objectEffect", 0x00AD5048u },
	{ "objectEffectGroup", 0x00AD506Cu },
	{ "objectEffectModifier", 0x00AD5090u },
	{ "objectEffectPackage", 0x00AD50B4u },
	{ "objectEffectPackageElem", 0x00AD50D8u },
	{ "overrideSpellData", 0x00AD42C8u },
	{ "package", 0x00AD42ECu },
	{ "pageTextMaterial", 0x00AD4310u },
	{ "paperDollItemFrame", 0x00AD4334u },
	{ "particleColor", 0x00AD4358u },
	{ "petPersonality", 0x00AD437Cu },
	{ "powerDisplay", 0x00AD43A0u },
	{ "pvpDifficulty", 0x00AD43C4u },
	{ "questFactionReward", 0x00AD43E8u },
	{ "questInfo", 0x00AD440Cu },
	{ "questSort", 0x00AD4430u },
	{ "questXP", 0x00AD4454u },
	{ "randPropPoints", 0x00AD449Cu },
	{ "resistances", 0x00AD4478u },
	{ "scalingStatDistribution", 0x00AD44C0u },
	{ "scalingStatValues", 0x00AD44E4u },
	{ "screenEffect", 0x00AD4508u },
	{ "serverMessages", 0x00AD452Cu },
	{ "sheatheSoundLookups", 0x00AD4550u },
	{ "skillCostsData", 0x00AD4574u },
	{ "skillLineAbility", 0x00AD4598u },
	{ "skillLineCategory", 0x00AD45BCu },
	{ "skillLine", 0x00AD45E0u },
	{ "skillRaceClassInfo", 0x00AD4604u },
	{ "skillTiers", 0x00AD4628u },
	{ "soundAmbience", 0x00AD464Cu },
	{ "soundEmitters", 0x00AD4694u },
	{ "soundEntriesAdvanced", 0x00AD5024u },
	{ "soundEntries", 0x00AD4670u },
	{ "soundFilter", 0x00AD50FCu },
	{ "soundFilterElem", 0x00AD5120u },
	{ "soundProviderPreferences", 0x00AD46B8u },
	{ "soundSamplePreferences", 0x00AD46DCu },
	{ "soundWaterType", 0x00AD4700u },
	{ "spamMessages", 0x00AD4724u },
	{ "spellCastTimes", 0x00AD4748u },
	{ "spellCategory", 0x00AD476Cu },
	{ "spellChainEffects", 0x00AD4790u },
	{ "spell", 0x00AD49D0u },
	{ "spellDescriptionVariables", 0x00AD47B4u },
	{ "spellDifficulty", 0x00AD47D8u },
	{ "spellDispelType", 0x00AD47FCu },
	{ "spellDuration", 0x00AD4820u },
	{ "spellEffectCameraShakes", 0x00AD4844u },
	{ "spellFocusObject", 0x00AD4868u },
	{ "spellIcon", 0x00AD488Cu },
	{ "spellItemEnchantmentCondition", 0x00AD48D4u },
	{ "spellItemEnchantment", 0x00AD48B0u },
	{ "spellMechanic", 0x00AD48F8u },
	{ "spellMissile", 0x00AD491Cu },
	{ "spellMissileMotion", 0x00AD4940u },
	{ "spellRadius", 0x00AD4964u },
	{ "spellRange", 0x00AD4988u },
	{ "spellRuneCost", 0x00AD49ACu },
	{ "spellShapeshiftForm", 0x00AD49F4u },
	{ "spellVisual", 0x00AD4AA8u },
	{ "spellVisualEffectName", 0x00AD4A18u },
	{ "spellVisualKitAreaModel", 0x00AD4A60u },
	{ "spellVisualKit", 0x00AD4A3Cu },
	{ "spellVisualKitModelAttach", 0x00AD4A84u },
	{ "stableSlotPrices", 0x00AD4ACCu },
	{ "startup_Strings", 0x00AB6350u },
	{ "stationery", 0x00AD4AF0u },
	{ "stringLookups", 0x00AD4B14u },
	{ "summonProperties", 0x00AD4B38u },
	{ "talent", 0x00AD4B5Cu },
	{ "talentTab", 0x00AD4B80u },
	{ "taxiNodes", 0x00AD4BA4u },
	{ "taxiPath", 0x00AD4BECu },
	{ "taxiPathNode", 0x00AD4BC8u },
	{ "teamContributionPoints", 0x00AD4C10u },
	{ "terrainType", 0x00AD4C34u },
	{ "terrainTypeSounds", 0x00AD4C58u },
	{ "totemCategory", 0x00AD4C7Cu },
	{ "transportAnimation", 0x00AD4CA0u },
	{ "transportPhysics", 0x00AD4CC4u },
	{ "transportRotation", 0x00AD4CE8u },
	{ "uISoundLookups", 0x00AD4D0Cu },
	{ "unitBlood", 0x00AD4D54u },
	{ "unitBloodLevels", 0x00AD4D30u },
	{ "vehicle", 0x00AD4D78u },
	{ "vehicleSeat", 0x00AD4D9Cu },
	{ "vehicleUIIndSeat", 0x00AD4DE4u },
	{ "vehicleUIIndicator", 0x00AD4DC0u },
	{ "videoHardware", 0x00ADBF88u },
	{ "vocalUISounds", 0x00AD4E08u },
	{ "wMOAreaTable", 0x00AD4E2Cu },
	{ "weaponImpactSounds", 0x00AD4E50u },
	{ "weaponSwingSounds2", 0x00AD4E74u },
	{ "weather", 0x00AD4E98u },
	{ "worldChunkSounds", 0x00AD5000u },
	{ "worldMapArea", 0x00AD4EBCu },
	{ "worldMapContinent", 0x00AD4EE0u },
	{ "worldMapOverlay", 0x00AD4F04u },
	{ "worldMapTransforms", 0x00AD4F28u },
	{ "worldSafeLocs", 0x00AD4F4Cu },
	{ "worldStateUI", 0x00AD4F70u },
	{ "worldStateZoneSounds", 0x00AD4FDCu },
	{ "zoneIntroMusicTable", 0x00AD4F94u },
	{ "zoneMusic", 0x00AD4FB8u },
};

enum DBCMode : uint8_t
{
	DBCMODE_MERGE = 0,
	DBCMODE_REPLACE_ALL = 1
};

constexpr uint32_t kStrSentinelNull = 0xFFFFFFFFu;

static char tolower_ascii(char c)
{
	return (c >= 'A' && c <= 'Z') ? char(c + 32) : c;
}

static bool iequals(const char* a, const char* b)
{
	for (; *a && *b; ++a, ++b)
		if (tolower_ascii(*a) != tolower_ascii(*b))
			return false;
	return *a == *b;
}

WoWClientDB* DBCPatch::FindStorage(const char* name)
{
	for (const DBCEntry& e : kStorages)
		if (iequals(e.name, name))
			return reinterpret_cast<WoWClientDB*>(e.base);
	return nullptr;
}

bool DBCPatch::EnsureCapacity(WoWClientDB* db, int wantMin, int wantMax)
{
	int newMin = db->Rows ? (wantMin < db->minIndex ? wantMin : db->minIndex) : wantMin;
	int newMax = db->Rows ? (wantMax > db->maxIndex ? wantMax : db->maxIndex) : wantMax;
	if (db->Rows && newMin == db->minIndex && newMax == db->maxIndex)
		return true; // already covered

	size_t newCount = static_cast<size_t>(newMax - newMin + 1);
	void** grown = reinterpret_cast<void**>(
	    SMem::Alloc(newCount * sizeof(void*), "DBCExpand", __LINE__, 0));
	if (!grown)
		return false;
	std::memset(grown, 0, newCount * sizeof(void*));

	if (db->Rows && db->maxIndex >= db->minIndex)
	{
		void** old = reinterpret_cast<void**>(db->Rows);
		int oldCount = db->maxIndex - db->minIndex + 1;
		for (int i = 0; i < oldCount; ++i)
			grown[(db->minIndex + i) - newMin] = old[i];
	}

	db->Rows = reinterpret_cast<int32_t*>(grown);
	db->minIndex = newMin;
	db->maxIndex = newMax;
	LOG_DEBUG << "DBC grow rows to [" << newMin << ".." << newMax << "] "
	          << (int)newCount << " slots (" << (int)(newCount * sizeof(void*)) << "B)";
	return true;
}

void** DBCPatch::SlotFor(WoWClientDB* db, uint32_t id)
{
	int iid = static_cast<int>(id);
	if (db->Rows && iid >= db->minIndex && iid <= db->maxIndex)
		return &reinterpret_cast<void**>(db->Rows)[iid - db->minIndex];
	return nullptr;
}

static bool RowsAreInlineStorage(void** byId, int idCount, const void* denseBase,
    int denseCount, uint32_t recordSize)
{
	if (!byId || !denseBase || idCount <= 0 || denseCount <= 0 || recordSize == 0)
		return false;
	uintptr_t lo = reinterpret_cast<uintptr_t>(denseBase);
	uintptr_t hi = lo + static_cast<size_t>(denseCount) * recordSize;
	for (int i = 0; i < idCount; ++i)
	{
		uintptr_t p = reinterpret_cast<uintptr_t>(byId[i]);
		if (p >= lo && p < hi && (p - lo) % recordSize == 0)
			return true;
	}
	return false;
}

void DBCPatch::RebuildInlineDense(WoWClientDB* db, void** byId, int idCount, size_t count,
    uint32_t recordSize)
{
	uint8_t* dense = static_cast<uint8_t*>(
	    SMem::Alloc(count * recordSize, "DBCDenseInline", __LINE__, 0));
	if (!dense)
		return;
	AddPatchedRange(dense, dense + count * recordSize);

	size_t w = 0;
	for (int i = 0; i < idCount; ++i)
	{
		if (!byId[i])
			continue;
		uint8_t* dst = dense + w * recordSize;
		std::memcpy(dst, byId[i], recordSize);
		byId[i] = dst;
		++w;
	}
	db->FirstRow = reinterpret_cast<int32_t*>(dense);
	db->numRows = static_cast<int>(count);
	LOG_DEBUG << "DBC dense rebuild (inline) rows=" << (int)count << " recSize=" << recordSize;
}

void DBCPatch::RebuildPointerDense(WoWClientDB* db, void** byId, int idCount, size_t count)
{
	void** dense = static_cast<void**>(
	    SMem::Alloc(count * sizeof(void*), "DBCDensePtrs", __LINE__, 0));
	if (!dense)
		return;

	size_t w = 0;
	for (int i = 0; i < idCount; ++i)
		if (byId[i])
			dense[w++] = byId[i];
	db->FirstRow = reinterpret_cast<int32_t*>(dense);
	db->numRows = static_cast<int>(count);
	LOG_DEBUG << "DBC dense rebuild (pointer) rows=" << (int)count;
}

void DBCPatch::RebuildDenseArray(WoWClientDB* db, uint32_t recordSize, bool inlineStorage)
{
	if (!db->Rows || db->maxIndex < db->minIndex || recordSize == 0)
		return;

	void** byId = reinterpret_cast<void**>(db->Rows);
	int idCount = db->maxIndex - db->minIndex + 1;
	size_t count = 0;
	for (int i = 0; i < idCount; ++i)
		if (byId[i])
			++count;
	if (count == 0)
		return;

	if (inlineStorage)
		RebuildInlineDense(db, byId, idCount, count, recordSize);
	else
		RebuildPointerDense(db, byId, idCount, count);
}

void DBCPatch::FixupStrings(void* record, const std::vector<uint16_t>& strOffsets,
    const char* internedBlob, uint32_t blobSize)
{
	uint8_t* rec = static_cast<uint8_t*>(record);
	for (uint16_t fieldOff : strOffsets)
	{
		uint32_t blobOff;
		std::memcpy(&blobOff, rec + fieldOff, sizeof(uint32_t));
		const char* str;
		if (blobOff == kStrSentinelNull || !internedBlob)
			str = "";
		else if (blobOff < blobSize)
			str = internedBlob + blobOff;
		else
			str = "";
		std::memcpy(rec + fieldOff, &str, sizeof(const char*));
	}
}
void DBCPatch::WriteRecord(void* dst, const uint8_t* image, uint32_t recordSize,
    const std::vector<uint16_t>& strOffsets,
    const char* blob, uint32_t blobSize)
{
	std::memcpy(dst, image, recordSize);
	FixupStrings(dst, strOffsets, blob, blobSize);
}

bool DBCPatch::ApplyRecords(const char* dbcName, uint32_t recordSize,
    const std::vector<uint16_t>& strOffsets,
    const std::vector<uint32_t>& ids, const uint8_t* images,
    const char* strBlock, uint32_t strBlockSize)
{
	WoWClientDB* db = FindStorage(dbcName);
	if (!db || !db->isLoaded || recordSize == 0)
	{
		if (recordSize != 0 && GlobalCDBCMap.hasCDBC(dbcName))
		{
			bool any = false;
			for (uint32_t i = 0; i < ids.size(); ++i)
			{
				const uint8_t* image = images + static_cast<size_t>(i) * recordSize;
				if (GlobalCDBCMap.patchRow(dbcName, static_cast<int>(ids[i]), image, recordSize))
					any = true;
				else
					Util::DebugOutput("DBC patch: CDBC '%s' rejected id %u (size %u)",
					    dbcName, ids[i], recordSize);
			}
			return any;
		}
		Util::DebugOutput("DBC patch: unknown/unloaded DBC '%s' (size %u)", dbcName, recordSize);
		LOG_DEBUG << "DBC patch unknown/unloaded '" << dbcName << "' size=" << recordSize
		          << " nativeDb=" << (db ? 1 : 0) << " allCDBCs=" << (int)GlobalCDBCMap.allCDBCs.size()
		          << " hasCDBC=" << (int)GlobalCDBCMap.hasCDBC(dbcName);
		return false;
	}

	int idSpan = db->maxIndex - db->minIndex + 1;
	bool inlineStorage = RowsAreInlineStorage(
	    reinterpret_cast<void**>(db->Rows), idSpan, db->FirstRow, db->numRows, recordSize);

	char* blob = nullptr;
	if (strBlockSize && strBlock)
	{
		blob = static_cast<char*>(SMem::Alloc(strBlockSize, "DBCRecordStrings", __LINE__, 0));
		if (!blob)
			return false;
		std::memcpy(blob, strBlock, strBlockSize);
		blob[strBlockSize - 1] = '\0';
	}

	size_t n = ids.size();
	if (n == 0)
		return true;

	int batchMin = static_cast<int>(ids[0]), batchMax = batchMin;
	for (size_t i = 1; i < n; ++i)
	{
		int v = static_cast<int>(ids[i]);
		if (v < batchMin)
			batchMin = v;
		if (v > batchMax)
			batchMax = v;
	}
	if (!EnsureCapacity(db, batchMin, batchMax))
		return false;

	uint8_t* block = static_cast<uint8_t*>(SMem::Alloc(n * recordSize, "DBCRecordBlock", __LINE__, 0));
	if (!block)
		return false;
	AddPatchedRange(block, block + n * recordSize);

	LOG_DEBUG << "DBC apply '" << dbcName << "' records=" << (int)n << " recSize=" << recordSize
	          << " ids=[" << batchMin << ".." << batchMax << "] block=" << (int)(n * recordSize)
	          << "B strBlob=" << (int)strBlockSize << "B";

	for (size_t i = 0; i < n; ++i)
	{
		void** slot = SlotFor(db, ids[i]);
		if (!slot)
			continue;
		uint8_t* rec = block + i * recordSize;
		WriteRecord(rec, images + i * recordSize, recordSize, strOffsets, blob, strBlockSize);
		*slot = rec;
	}

	RebuildDenseArray(db, recordSize, inlineStorage);
	return true;
}
