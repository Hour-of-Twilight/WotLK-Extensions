#include "UnitHealthPrediction.h"
#include "AuraValuesCache.h"
#include "Packet.h"

#include <ClientData/Aura.h>
#include <CustomLua.h>
#include <CustomPacket.h>
#include <Lua/XMLExtensions.h>

#include <Windows.h>

using ClientData::Aura::MaxSpellEffects;

static constexpr uint32_t kIncomingHealGraceMs = 1500;

UnitHealthPrediction& UnitHealthPrediction::Instance()
{
	static UnitHealthPrediction instance;
	return instance;
}

void UnitHealthPrediction::ClearAll()
{
	m_incomingHeals.clear();
}

const UnitHealthPrediction::AbsorbEffects& UnitHealthPrediction::EffectsFor(uint32_t spellId)
{
	auto it = m_spellEffects.find(spellId);
	if (it != m_spellEffects.end())
		return it->second;

	AbsorbEffects effects;
	SpellRow row;
	if (ClientDB::GetSpellRow(spellId, row))
	{
		for (uint32_t i = 0; i < MaxSpellEffects; ++i)
		{
			uint32_t auraType = row.m_effectAura[i];
			if (auraType == ClientData::Aura::SPELL_AURA_SCHOOL_ABSORB || auraType == ClientData::Aura::SPELL_AURA_MANA_SHIELD)
				effects.absorbMask |= static_cast<uint8_t>(1u << i);
			else if (auraType == ClientData::Aura::SPELL_AURA_SCHOOL_HEAL_ABSORB)
				effects.healAbsorbMask |= static_cast<uint8_t>(1u << i);
		}
	}

	return m_spellEffects.emplace(spellId, effects).first->second;
}

uint32_t UnitHealthPrediction::SumAbsorbs(uint64_t guid, bool healAbsorb)
{
	if (!guid)
		return 0;

	CGUnit* unit = static_cast<CGUnit*>(ClntObjMgr::ObjectPtr(guid, TYPEMASK_UNIT));
	if (!unit)
		return 0;

	int count = CGUnit_C::GetAuraCount(unit);
	if (count > static_cast<int>(ClientData::Aura::MaxAuraSlots))
		count = static_cast<int>(ClientData::Aura::MaxAuraSlots);

	uint32_t total = 0;
	for (int slot = 0; slot < count; ++slot)
	{
		AuraData* aura = CGUnit_C::GetAura(unit, static_cast<uint32_t>(slot));
		if (!aura || !aura->spellId)
			continue;

		const AbsorbEffects& effects = EffectsFor(aura->spellId);
		uint8_t mask = healAbsorb ? effects.healAbsorbMask : effects.absorbMask;
		if (!mask)
			continue;

		AuraValuesCache::Values values;
		if (!sAuraValuesCache.GetSlotValues(guid, static_cast<uint8_t>(slot), aura->spellId, values))
			continue;

		for (uint32_t i = 0; i < MaxSpellEffects; ++i)
		{
			uint8_t bit = static_cast<uint8_t>(1u << i);
			if ((mask & bit) && (values.effectMask & bit) && values.amounts[i] > 0)
				total += static_cast<uint32_t>(values.amounts[i]);
		}
	}

	return total;
}

uint32_t UnitHealthPrediction::GetTotalAbsorbs(uint64_t guid)
{
	return SumAbsorbs(guid, false);
}

uint32_t UnitHealthPrediction::GetTotalHealAbsorbs(uint64_t guid)
{
	return SumAbsorbs(guid, true);
}

uint32_t UnitHealthPrediction::GetIncomingHeals(uint64_t guid, uint64_t healerGuid)
{
	auto it = m_incomingHeals.find(guid);
	if (it == m_incomingHeals.end())
		return 0;

	uint32_t now = GetTickCount();
	uint32_t total = 0;
	std::vector<IncomingHeal>& heals = it->second;
	for (auto heal = heals.begin(); heal != heals.end();)
	{
		if (static_cast<int32_t>(now - heal->expiresMs) >= 0)
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

void UnitHealthPrediction::SignalUnitEvent(const char* eventName, uint64_t guid)
{
	int count = 0;
	const char** tokens = Script_GetTokensFromGUID(&guid, &count);
	if (!tokens)
		return;

	for (int i = 0; i < count; ++i)
		FrameXMLExtensions::SignalEvent(eventName, "%s", tokens[i]);
}

void UnitHealthPrediction::OnAuraValuesReceived(uint64_t guid, uint32_t spellId)
{
	const AbsorbEffects& effects = EffectsFor(spellId);
	if (effects.absorbMask)
		SignalUnitEvent("UNIT_ABSORB_AMOUNT_CHANGED", guid);
	if (effects.healAbsorbMask)
		SignalUnitEvent("UNIT_HEAL_ABSORB_AMOUNT_CHANGED", guid);
}

void UnitHealthPrediction::Handler_SMSG_UNIT_HEAL_PREDICTION(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint64_t caster = r.GetUInt64();
	uint64_t target = r.GetUInt64();
	uint32_t spellId = r.GetUInt32();
	int32_t amount = r.GetInt32();
	uint32_t remainingCastMs = r.GetUInt32();

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
		heal.amount = static_cast<uint32_t>(amount);
		heal.expiresMs = GetTickCount() + remainingCastMs + kIncomingHealGraceMs;
		heals.push_back(heal);
	}

	if (heals.empty())
		self.m_incomingHeals.erase(target);

	SignalUnitEvent("UNIT_HEAL_PREDICTION", target);
}

static bool ResolveUnitToken(lua_State* L, int index, uint64_t& guid)
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
	uint64_t guid = 0;
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
	uint64_t guid = 0;
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
	uint64_t guid = 0;
	if (!ResolveUnitToken(L, 1, guid))
	{
		FrameScript::PushNil(L);
		return 1;
	}

	uint64_t healerGuid = 0;
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
