#include <Defines.h>
#include "FrameDetours.h"
#include <ClientDetours.h>
#include <SharedDefines.h>
#include <Logger.h>
#include <ClientData/WowObjects.h>
#include "XMLExtensions.h"
#include "MultiCastBarDetours.h"
#include "Streaming/BackgroundDownloader.h"
#include "Tools/MPQScanner.h"
#include "Packets/CraftingPackets.h"
#include "Packets/GemSocketPackets.h"

CLIENT_DETOUR(CGlueMgr__Idle, 0x004DAB40, __cdecl, int, (void))
{
	int r = CGlueMgr__Idle();
	sBackgroundDownloader.PumpMainThread();
	sMpqScanner.Pump();
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

CLIENT_FUNCTION(ClntObjMgrObjectPtr_Full, 0x004D4DB0, __cdecl, void*, (uint32 guidLow, uint32 guidHigh, uint32 typeMask, const char* file, uint32 line))

CLIENT_DETOUR(CGItemSocketInfo__SetGem, 0x005C4780, __cdecl, int, (uint32 socketIndex, uint32 guidLow, uint32 guidHigh, int arg_C, int arg_10, char arg_14))
{
	if (socketIndex >= 7)
		return 0;

	if (!(*g_socketInfoGuidLow | *g_socketInfoGuidHigh))
		return 0;

	if (!*g_socketInfoPtr)
		return 0;

	uint32* gemGuidLow = reinterpret_cast<uint32*>(0xC20FC8 + socketIndex * 8);
	uint32* gemGuidHigh = reinterpret_cast<uint32*>(0xC20FCC + socketIndex * 8);
	uint32* gemArg1 = reinterpret_cast<uint32*>(0xC20FB0 + socketIndex * 8);
	uint32* gemArg2 = reinterpret_cast<uint32*>(0xC20FB4 + socketIndex * 8);
	uint8* gemFlags = reinterpret_cast<uint8*>(0xC20FA8 + socketIndex);

	if (!(guidLow | guidHigh))
	{
		*gemGuidLow = 0;
		*gemGuidHigh = 0;
		FrameXMLExtensions::SignalEvent("SOCKET_INFO_UPDATE", "");
		return 1;
	}

	uint64 guid = (static_cast<uint64>(guidHigh) << 32) | guidLow;
	void* obj = ClntObjMgrObjectPtr_Full(guidLow, guidHigh, 2, ".\\ItemSocketInfo.cpp", 0x60);
	if (!obj)
		return 0;

	ClientData::WowItem item(obj);
	if (!item)
		return 0;

	uint32 itemEntry = item.Entry();

	uint32 var_8 = 0;
	void* pCacheDB = WDB_CACHE_ITEM;
	ItemCache* itemStats = DBItemCache_GetInfoBlockByID(pCacheDB, itemEntry, nullptr, nullptr, 0, 0);
	if (!itemStats)
		return 0;

	uint32 gemPropertiesID = static_cast<uint32>(itemStats->GemProperties);
	if (!gemPropertiesID)
		return 0;

	void* gemPropertiesBase = reinterpret_cast<uint8*>(g_gemPropertiesDB) + 0x18;
	void* gemRecord = WowClientDB_GemProperties_GetRecord(gemPropertiesBase, gemPropertiesID);
	if (!gemRecord)
		return 0;

	uint32 socketColor = *reinterpret_cast<uint32*>(reinterpret_cast<uint8*>(*g_socketInfoPtr) + socketIndex * 4 + 0x1C0);
	uint32 gemColor = *reinterpret_cast<uint32*>(reinterpret_cast<uint8*>(gemRecord) + 0x10);

	if (socketColor != 1 && !(socketColor & gemColor))
	{
		CGGameUI::DisplayError(0x22A);
		return 0;
	}

	*gemGuidHigh = guidHigh;
	*gemGuidLow = guidLow;
	*gemArg1 = static_cast<uint32>(arg_C);
	*gemArg2 = static_cast<uint32>(arg_10);
	*gemFlags = static_cast<uint8>(arg_14);

	FrameXMLExtensions::SignalEvent("SOCKET_INFO_UPDATE", nullptr);
	return 1;
}

void __cdecl CGSpellBook__AddKnownSpell_PutActionInSlotHook(int slot)
{
	uint32 spellId = CGGameUI::GetCursorSpell();
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
	for (uint32 i = 0; i < 0x90 && !found; ++i)
	{
		uint32 category = 0;
		uint32 slotSpell = CGActionBar::GetSpell(i, &category);
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
		FrameXMLExtensions::SignalEvent("HOT_PLACE_ACTION", "%u%u", spellId, (uint32)slot);
	CGGameUI::ClearCursor(1, 1);
}

static constexpr int kCursorResetModeRepair = 0x11;

static bool ClientOwnsContainerItemClick(lua_State* L)
{
	if (FrameScript::Type(L, 3) == ClientData::LUA_TSTRING)
		return true;

	if (Spell_C::IsTargeting())
		return true;

	if (CGGameUI::CursorGetResetMode() == kCursorResetModeRepair)
		return true;

	if (*CGBankInfo__m_banker || *CGMerchantInfo__m_merchant)
		return true;

	if (*CGMailInfo__m_sendMailShowing && *CGMailInfo__m_object)
		return true;

	if (*CGAuctionHouse__m_auctionsTabShowing && *CGAuctionHouse__m_auctioneer)
		return true;

	return *CGTradeInfo__m_tradingPlayer || *CGGuildBankInfo__m_banker;
}

CLIENT_DETOUR(Script_UseContainerItem, 0x005D8650, __cdecl, int, (lua_State * L))
{
	if (ClientOwnsContainerItemClick(L))
		return Script_UseContainerItem(L);

	if (sCraftingPackets.HandleUseContainerItem(L) || sGemSocketPackets.HandleUseContainerItem(L))
		return 0;

	return Script_UseContainerItem(L);
}

CLIENT_DETOUR(FrameXML_CheckSignature, 0x008165E0, __cdecl, int, (const char* tocPath, const char* bindingsPath, const void* publicKey, void* md5Out))
{
	FrameXML_CheckSignature(tocPath, bindingsPath, publicKey, md5Out);
	return 3;
}
