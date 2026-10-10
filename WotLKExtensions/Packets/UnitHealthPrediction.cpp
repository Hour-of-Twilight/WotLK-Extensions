#include <Defines.h>
#include "UnitHealthPrediction.h"
#include "AuraValuesCache.h"
#include "Packet.h"

#include <ClientData/Aura.h>
#include <ClientData/WowObjects.h>
#include <CustomLua.h>
#include <CustomPacket.h>
#include <Lua/XMLExtensions.h>

#include <Windows.h>

using ClientData::Aura::MaxSpellEffects;

static constexpr uint32 kIncomingHealGraceMs = 1500;

UnitHealthPrediction& UnitHealthPrediction::Instance()
{
	static UnitHealthPrediction instance;
	return instance;
}

void UnitHealthPrediction::ClearAll()
{
	m_incomingHeals.clear();
}

const UnitHealthPrediction::AbsorbEffects& UnitHealthPrediction::EffectsFor(uint32 spellId)
{
	auto it = m_spellEffects.find(spellId);
	if (it != m_spellEffects.end())
		return it->second;

	AbsorbEffects effects;
	SpellRow row;
	if (ClientDB::GetSpellRow(spellId, row))
	{
		for (uint32 i = 0; i < MaxSpellEffects; ++i)
		{
			uint32 auraType = row.m_effectAura[i];
			if (auraType == ClientData::Aura::SPELL_AURA_SCHOOL_ABSORB || auraType == ClientData::Aura::SPELL_AURA_MANA_SHIELD)
				effects.absorbMask |= static_cast<uint8>(1u << i);
			else if (auraType == ClientData::Aura::SPELL_AURA_SCHOOL_HEAL_ABSORB)
				effects.healAbsorbMask |= static_cast<uint8>(1u << i);
		}
	}

	return m_spellEffects.emplace(spellId, effects).first->second;
}

uint32 UnitHealthPrediction::SumAbsorbs(uint64 guid, bool healAbsorb)
{
	if (!guid)
		return 0;

	ClientData::WowUnit unit = ClientData::WowUnit::FromGuid(guid);
	if (!unit)
		return 0;

	int count = unit.AuraCount();
	if (count > static_cast<int>(ClientData::Aura::MaxAuraSlots))
		count = static_cast<int>(ClientData::Aura::MaxAuraSlots);

	uint32 total = 0;
	for (int slot = 0; slot < count; ++slot)
	{
		AuraData* aura = unit.Aura(static_cast<uint32>(slot));
		if (!aura || !aura->spellId)
			continue;

		const AbsorbEffects& effects = EffectsFor(aura->spellId);
		uint8 mask = healAbsorb ? effects.healAbsorbMask : effects.absorbMask;
		if (!mask)
			continue;

		AuraValuesCache::Values values;
		if (!sAuraValuesCache.GetSlotValues(guid, static_cast<uint8>(slot), aura->spellId, values))
			continue;

		for (uint32 i = 0; i < MaxSpellEffects; ++i)
		{
			uint8 bit = static_cast<uint8>(1u << i);
			if ((mask & bit) && (values.effectMask & bit) && values.amounts[i] > 0)
				total += static_cast<uint32>(values.amounts[i]);
		}
	}

	return total;
}

uint32 UnitHealthPrediction::GetTotalAbsorbs(uint64 guid)
{
	return SumAbsorbs(guid, false);
}

uint32 UnitHealthPrediction::GetTotalHealAbsorbs(uint64 guid)
{
	return SumAbsorbs(guid, true);
}

uint32 UnitHealthPrediction::GetIncomingHeals(uint64 guid, uint64 healerGuid)
{
	auto it = m_incomingHeals.find(guid);
	if (it == m_incomingHeals.end())
		return 0;

	uint32 now = GetTickCount();
	uint32 total = 0;
	std::vector<IncomingHeal>& heals = it->second;
	for (auto heal = heals.begin(); heal != heals.end();)
	{
		if (static_cast<int32>(now - heal->expiresMs) >= 0)
		{
			heal = heals.erase(heal);
			continue;
		}

		if (!healerGuid || heal->caster == healerGuid)
			total += heal->amount;
		++heal;
	}

	if (heals.empty())
		m_incomingHeals.erase(it);

	return total;
}

void UnitHealthPrediction::SignalUnitEvent(const char* eventName, uint64 guid)
{
	int count = 0;
	const char** tokens = Script_GetTokensFromGUID(&guid, &count);
	if (!tokens)
		return;

	for (int i = 0; i < count; ++i)
		FrameXMLExtensions::SignalEvent(eventName, "%s", tokens[i]);
}

void UnitHealthPrediction::OnAuraValuesReceived(uint64 guid, uint32 spellId)
{
	const AbsorbEffects& effects = EffectsFor(spellId);
	if (effects.absorbMask)
		SignalUnitEvent("UNIT_ABSORB_AMOUNT_CHANGED", guid);
	if (effects.healAbsorbMask)
		SignalUnitEvent("UNIT_HEAL_ABSORB_AMOUNT_CHANGED", guid);
}

void UnitHealthPrediction::Handler_SMSG_UNIT_HEAL_PREDICTION(void*, uint32, uint32, CDataStore* pkt)
{
	Packet r(pkt);
	uint64 caster = r.GetUInt64();
	uint64 target = r.GetUInt64();
	uint32 spellId = r.GetUInt32();
	int32 amount = r.GetInt32();
	uint32 remainingCastMs = r.GetUInt32();

	if (!caster || !target)
		return;

	UnitHealthPrediction& self = sUnitHealthPrediction;
	std::vector<IncomingHeal>& heals = self.m_incomingHeals[target];
	for (auto heal = heals.begin(); heal != heals.end();)
	{
		if (heal->caster == caster)
			heal = heals.erase(heal);
		else
			++heal;
	}

	if (amount > 0)
	{
		IncomingHeal heal;
		heal.caster = caster;
		heal.spellId = spellId;
		heal.amount = static_cast<uint32>(amount);
		heal.expiresMs = GetTickCount() + remainingCastMs + kIncomingHealGraceMs;
		heals.push_back(heal);
	}

	if (heals.empty())
		self.m_incomingHeals.erase(target);

	SignalUnitEvent("UNIT_HEAL_PREDICTION", target);
}

static bool ResolveUnitToken(lua_State* L, int index, uint64& guid)
{
	guid = 0;
	if (FrameScript::GetTop(L) < index)
		return false;

	const char* token = FrameScript::ToLString(L, index, false);
	if (!token || !*token)
		return false;

	Script_GetGUIDFromToken(token, &guid, 0);
	return guid != 0;
}

int UnitHealthPrediction::Script_UnitGetTotalAbsorbs(lua_State* L)
{
	uint64 guid = 0;
	if (!ResolveUnitToken(L, 1, guid))
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::PushNumber(L, sUnitHealthPrediction.GetTotalAbsorbs(guid));
	return 1;
}

int UnitHealthPrediction::Script_UnitGetTotalHealAbsorbs(lua_State* L)
{
	uint64 guid = 0;
	if (!ResolveUnitToken(L, 1, guid))
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::PushNumber(L, sUnitHealthPrediction.GetTotalHealAbsorbs(guid));
	return 1;
}

int UnitHealthPrediction::Script_UnitGetIncomingHeals(lua_State* L)
{
	uint64 guid = 0;
	if (!ResolveUnitToken(L, 1, guid))
	{
		FrameScript::PushNil(L);
		return 1;
	}

	uint64 healerGuid = 0;
	if (FrameScript::GetTop(L) >= 2 && !ResolveUnitToken(L, 2, healerGuid))
	{
		FrameScript::PushNumber(L, 0);
		return 1;
	}

	FrameScript::PushNumber(L, sUnitHealthPrediction.GetIncomingHeals(guid, healerGuid));
	return 1;
}

void UnitHealthPrediction::RegisterLuaFunctions()
{
	sLua.RegisterFunction("UnitGetTotalAbsorbs", &Script_UnitGetTotalAbsorbs, LuaFunctionState::FRAME);
	sLua.RegisterFunction("UnitGetTotalHealAbsorbs", &Script_UnitGetTotalHealAbsorbs, LuaFunctionState::FRAME);
	sLua.RegisterFunction("UnitGetIncomingHeals", &Script_UnitGetIncomingHeals, LuaFunctionState::FRAME);
}

void UnitHealthPrediction::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_UNIT_HEAL_PREDICTION, &Handler_SMSG_UNIT_HEAL_PREDICTION);
}
