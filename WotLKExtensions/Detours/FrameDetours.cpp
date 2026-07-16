#include "FrameDetours.h"
#include <ClientDetours.h>
#include <SharedDefines.h>
#include <Logger.h>
#include "XMLExtensions.h"
#include "MultiCastBarDetours.h"
#include "Streaming/BackgroundDownloader.h"


CLIENT_DETOUR(CGlueMgr__Idle, 0x004DAB40, __cdecl, int, (void))
{
	int r = CGlueMgr__Idle();
	sBackgroundDownloader.PumpMainThread();
	return r;
}

CLIENT_DETOUR(UpdateUsableAction, 0x005A9E20, __cdecl, bool, (uint32 slot, bool* outNoMana))
{
	bool result = UpdateUsableAction(slot, outNoMana);

	if (!result || !CGActionBar::IsSpell(slot))
		return result;

	uint32 category = 0;
	uint32 spellId = CGActionBar::GetSpell(slot, &category);

	if (spellId && !CGPlayer_C::HasSpell(spellId))
	{
		*outNoMana = false;
		return false;
	}

	return result;
}

CLIENT_FUNCTION(ClntObjMgrObjectPtr_Full, 0x004D4DB0, __cdecl, void*, (uint32_t guidLow, uint32_t guidHigh, uint32_t typeMask, const char* file, uint32_t line))

CLIENT_DETOUR(CGItemSocketInfo__SetGem, 0x005C4780, __cdecl, int, (uint32_t socketIndex, uint32_t guidLow, uint32_t guidHigh, int arg_C, int arg_10, char arg_14))
{
	if (socketIndex >= 7)
		return 0;

	if (!(*g_socketInfoGuidLow | *g_socketInfoGuidHigh))
		return 0;

	if (!*g_socketInfoPtr)
		return 0;

	uint32_t* gemGuidLow = reinterpret_cast<uint32_t*>(0xC20FC8 + socketIndex * 8);
	uint32_t* gemGuidHigh = reinterpret_cast<uint32_t*>(0xC20FCC + socketIndex * 8);
	uint32_t* gemArg1 = reinterpret_cast<uint32_t*>(0xC20FB0 + socketIndex * 8);
	uint32_t* gemArg2 = reinterpret_cast<uint32_t*>(0xC20FB4 + socketIndex * 8);
	uint8_t* gemFlags = reinterpret_cast<uint8_t*>(0xC20FA8 + socketIndex);

	if (!(guidLow | guidHigh))
	{
		*gemGuidLow = 0;
		*gemGuidHigh = 0;
		FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("SOCKET_INFO_UPDATE"), "");
		return 1;
	}

	uint64_t guid = (static_cast<uint64_t>(guidHigh) << 32) | guidLow;
	void* obj = ClntObjMgrObjectPtr_Full(guidLow, guidHigh, 2, ".\\ItemSocketInfo.cpp", 0x60);
	if (!obj)
		return 0;

	uint32_t* objFields = *reinterpret_cast<uint32_t**>(reinterpret_cast<uint8_t*>(obj) + 8);
	if (!objFields)
		return 0;

	uint32_t itemEntry = objFields[3];

	uint32_t var_8 = 0;
	void* pCacheDB = WDB_CACHE_ITEM;
	ItemCache* itemStats = DBItemCache_GetInfoBlockByID(pCacheDB, itemEntry, nullptr, nullptr, 0, 0);
	if (!itemStats)
		return 0;

	uint32_t gemPropertiesID = static_cast<uint32_t>(itemStats->GemProperties);
	if (!gemPropertiesID)
		return 0;

	void* gemPropertiesBase = reinterpret_cast<uint8_t*>(g_gemPropertiesDB) + 0x18;
	void* gemRecord = WowClientDB_GemProperties_GetRecord(gemPropertiesBase, gemPropertiesID);
	if (!gemRecord)
		return 0;

	uint32_t socketColor = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(*g_socketInfoPtr) + socketIndex * 4 + 0x1C0);
	uint32_t gemColor = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(gemRecord) + 0x10);

	if (socketColor != 1 && !(socketColor & gemColor))
	{
		CGGameUI::DisplayError(0x22A);
		return 0;
	}

	*gemGuidHigh = guidHigh;
	*gemGuidLow = guidLow;
	*gemArg1 = static_cast<uint32_t>(arg_C);
	*gemArg2 = static_cast<uint32_t>(arg_10);
	*gemFlags = static_cast<uint8_t>(arg_14);

	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("SOCKET_INFO_UPDATE"), nullptr);
	return 1;
}

void __cdecl CGSpellBook__AddKnownSpell_PutActionInSlotHook(int slot)
{
	uint32_t spellId = CGGameUI::GetCursorSpell();
	if (!spellId)
	{
		CGGameUI::ClearCursor(1, 1);
		return;
	}


	if (MultiCastBar_IsTotemSpell(spellId))
	{
		CGGameUI::ClearCursor(1, 1);
		return;
	}

	// SpellRow newRow;
	// WowClientDb_C::GetRow(&newRow);
	// bool hasNewRow = ClientDB::GetLocalizedRow(g_SpellDB, spellId, &newRow) != 0;

	bool found = false;
	for (uint32_t i = 0; i < 0x90 && !found; ++i)
	{
		uint32_t category = 0;
		uint32_t slotSpell = CGActionBar::GetSpell(i, &category);
		if (slotSpell == 0)
			continue;

		if (slotSpell == spellId)
		{
			found = true;
			break;
		}

		/*if (hasNewRow && newRow.m_name_lang)
		{
		    SpellRow slotRow;
		    WowClientDb_C::GetRow(&slotRow);
		    if (ClientDB::GetLocalizedRow(g_SpellDB, slotSpell, &slotRow) && slotRow.m_name_lang)
		    {
		        if (SStr::SStrCmp(newRow.m_name_lang, slotRow.m_name_lang, 0x7FFFFFFF) == 0)
		            found = true;
		    }
		    WowClientDb_C::nullsub_3(&slotRow);
		}*/
	}

	// WowClientDb_C::nullsub_3(&newRow);

	if (!found)
		FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_PLACE_ACTION"), "%u%u", spellId, (uint32_t)slot);
	else
		CGGameUI::ClearCursor(1, 1);
}
