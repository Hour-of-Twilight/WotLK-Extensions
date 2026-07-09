#pragma once
#include <Defines.h>
#include <flag96.h>
#include <PatchConfig.h>
#include <Util.h>

constexpr uint32 MAX_AFFIXES = 4;
// constexpr uint32 MAX_SPELL_SCHOOL = 7;
constexpr uint32 DLL_VER = 2;

static uint32_t dummy = 0;

#include <ClientData/Enums.h>
#include <ClientData/ObjectModel.h>
#include <ClientData/DBCRows.h>
#include <ClientData/GameStructs.h>
#include <ClientData/ClientFunctions.h>
#include <ClientData/ClientAddresses.h>

namespace ClientDB
{
	inline bool GetSpellRow(uint32_t spellId, SpellRow& out)
	{
		WowClientDb_C::GetRow(&out);
		if (!GetLocalizedRow(g_SpellDB, spellId, &out))
		{
			WowClientDb_C::nullsub_3(&out);
			return false;
		}
		return true;
	}
}
