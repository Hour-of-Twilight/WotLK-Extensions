#include "AuraValuesCache.h"
#include "Packet.h"

#include <CustomPacket.h>
#include <ClientData/ClientFunctions.h>
#include <Detours/ClientDetours.h>
#include <Lua/XMLExtensions.h>
#include <Util.h>

#include <Windows.h>

using ClientData::Aura::MaxSpellEffects;
using ClientData::Aura::TooltipCall;

static constexpr uint32_t kRequestIntervalMs = 1000;
static constexpr uint32_t kStaleAfterMs = 3000;

static constexpr size_t kPruneUnitThreshold = 256;
static constexpr uint32_t kPruneAgeMs = 60000;

AuraValuesCache& AuraValuesCache::Instance()
{
	static AuraValuesCache instance;
	return instance;
}

AuraValuesCache::Entry& AuraValuesCache::EntryFor(uint64_t guid, uint8_t slot)
{
	return m_entries[guid][slot];
}

void AuraValuesCache::ClearUnit(uint64_t guid)
{
	m_entries.erase(guid);
}

void AuraValuesCache::ClearAll()
{
	m_entries.clear();
	m_pending = TooltipCall{};
	m_active = TooltipCall{};
	m_displayed = TooltipCall{};
}

void AuraValuesCache::PruneIfLarge()
{
	if (m_entries.size() < kPruneUnitThreshold)
		return;

	uint32_t now = GetTickCount();
	for (auto unit = m_entries.begin(); unit != m_entries.end();)
	{
		for (auto slot = unit->second.begin(); slot != unit->second.end();)
		{
			if ((now - slot->second.receivedMs) > kPruneAgeMs)
				slot = unit->second.erase(slot);
			else
				++slot;
		}

		if (unit->second.empty())
			unit = m_entries.erase(unit);
		else
			++unit;
	}
}

void AuraValuesCache::RecordTooltipCall(CGUnit* unit, int32_t slot)
{
	TooltipCall& pending = sAuraValuesCache.m_pending;
	pending = TooltipCall{};

	if (!unit || slot < 0 || slot >= static_cast<int32_t>(ClientData::Aura::MaxAuraSlots))
		return;

	if (!unit->objectBase.ObjectData)
		return;

	pending.unitGuid = unit->objectBase.ObjectData->OBJECT_FIELD_GUID;
	pending.slot = slot;
	pending.valid = true;
}

void AuraValuesCache::BeginTooltip(void* tooltip, uint32_t spellId)
{
	m_active = m_pending;
	m_pending = TooltipCall{};

	m_active.tooltip = tooltip;
	m_active.spellId = spellId;
}

void AuraValuesCache::EndTooltip()
{
	m_displayed = m_active;
	m_active = TooltipCall{};
}

static void __cdecl RecordTooltipCallThunk(CGUnit* unit, int32_t slot)
{
	AuraValuesCache::RecordTooltipCall(unit, slot);
}

__declspec(naked) void CGTooltip__SetBuff_Hook()
{
	__asm {
        pushad
        pushfd
        push edi
        push esi
        call RecordTooltipCallThunk
        add  esp, 8
        popfd
        popad
        push 0x00625350
        ret
	}
}

CLIENT_DETOUR_THISCALL(CGTooltip__SetBuff, 0x00625350, void,
    (uint32_t spellId, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6))
{
	sAuraValuesCache.BeginTooltip(self, spellId);
	CGTooltip__SetBuff(self, spellId, a2, a3, a4, a5, a6);
	sAuraValuesCache.EndTooltip();
}

const AuraValuesCache::Values* AuraValuesCache::ResolveActive(uint32_t spellId)
{
	if (!m_active.valid || m_active.spellId != spellId)
		return nullptr;

	uint32_t now = GetTickCount();
	Entry& entry = EntryFor(m_active.unitGuid, static_cast<uint8_t>(m_active.slot));
	bool stale = !entry.haveData || (now - entry.receivedMs) >= kStaleAfterMs;
	if (stale && (now - entry.lastRequestMs) >= kRequestIntervalMs)
	{
		entry.lastRequestMs = now;
		Packet(CMSG_AURA_VALUES_REQUEST).PutUInt64(m_active.unitGuid).PutUInt8(static_cast<uint8_t>(m_active.slot)).Send();
	}

	if (!entry.haveData || entry.values.spellId != spellId)
		return nullptr;

	return &entry.values;
}

bool AuraValuesCache::GetActiveAmount(uint32_t spellId, uint32_t effectIndex, int32_t& out)
{
	if (effectIndex < 1 || effectIndex > MaxSpellEffects)
		return false;

	const Values* values = ResolveActive(spellId);
	if (!values)
		return false;

	uint32_t index = effectIndex - 1;
	if (!(values->effectMask & (1u << index)))
		return false;

	out = values->amounts[index];
	return true;
}

bool AuraValuesCache::GetActiveAbsorb(uint32_t spellId, const SpellRow* spell, int32_t& out)
{
	const Values* values = ResolveActive(spellId);
	if (!values)
		return false;

	if (spell)
	{
		for (uint32_t i = 0; i < MaxSpellEffects; ++i)
		{
			uint32_t auraType = spell->m_effectAura[i];
			bool absorbs = auraType == ClientData::Aura::SPELL_AURA_SCHOOL_ABSORB || auraType == ClientData::Aura::SPELL_AURA_MANA_SHIELD;
			if (absorbs && (values->effectMask & (1u << i)))
			{
				out = values->amounts[i];
				return true;
			}
		}
	}

	for (uint32_t i = 0; i < MaxSpellEffects; ++i)
	{
		if (values->effectMask & (1u << i))
		{
			out = values->amounts[i];
			return true;
		}
	}

	return false;
}

bool AuraValuesCache::GetActiveStacks(uint32_t spellId, int32_t& out)
{
	const Values* values = ResolveActive(spellId);
	if (!values)
		return false;

	out = values->stacks;
	return true;
}

void AuraValuesCache::Handler_SMSG_AURA_VALUES(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint64_t guid = r.GetUInt64();
	uint8_t slot = r.GetUInt8();
	uint32_t spellId = r.GetUInt32();
	uint8_t effectMask = r.GetUInt8();
	uint8_t stacks = r.GetUInt8();

	if (!guid || slot >= ClientData::Aura::MaxAuraSlots)
		return;

	Values values;
	values.spellId = spellId;
	values.effectMask = effectMask;
	values.stacks = stacks;
	for (uint32_t i = 0; i < MaxSpellEffects; ++i)
	{
		if (effectMask & (1u << i))
			values.amounts[i] = r.GetInt32();
	}

	sAuraValuesCache.PruneIfLarge();

	Entry& entry = sAuraValuesCache.EntryFor(guid, slot);
	entry.values = values;
	entry.receivedMs = GetTickCount();
	entry.haveData = true;

	TooltipCall& displayed = sAuraValuesCache.m_displayed;
	if (!displayed.valid || displayed.unitGuid != guid || displayed.slot != slot || displayed.spellId != spellId)
		return;

	if (!ClientData::Aura::IsFrameVisible(displayed.tooltip))
		return;

	FrameXMLExtensions::SignalEvent("HOT_AURA_VALUES_UPDATED", "");
}

void AuraValuesCache::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_AURA_VALUES, &Handler_SMSG_AURA_VALUES);

	uint32_t rel = reinterpret_cast<uint32_t>(&CGTooltip__SetBuff_Hook) - (ClientData::Aura::SetBuffCallSite + 5);
	Util::OverwriteUInt32AtAddress(ClientData::Aura::SetBuffCallSite + 1, rel);
}
