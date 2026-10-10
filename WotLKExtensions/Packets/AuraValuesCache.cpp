#include <Defines.h>
#include "AuraValuesCache.h"
#include "Packet.h"

#include <CustomPacket.h>
#include <ClientData/ClientFunctions.h>
#include <ClientData/WowObjects.h>
#include <Detours/ClientDetours.h>
#include <Lua/XMLExtensions.h>
#include <Packets/UnitHealthPrediction.h>
#include <Util.h>

#include <Windows.h>

using ClientData::Aura::MaxSpellEffects;
using ClientData::Aura::TooltipCall;

static constexpr uint32 kRequestIntervalMs = 1000;
static constexpr uint32 kStaleAfterMs = 3000;
static constexpr uint32 kMaxBackoffShift = 5;

static constexpr size_t kPruneUnitThreshold = 256;
static constexpr uint32 kPruneAgeMs = 60000;

AuraValuesCache& AuraValuesCache::Instance()
{
	static AuraValuesCache instance;
	return instance;
}

AuraValuesCache::Entry& AuraValuesCache::EntryFor(uint64 guid, uint8 slot)
{
	return m_entries[guid][slot];
}

void AuraValuesCache::ClearUnit(uint64 guid)
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

	uint32 now = GetTickCount();
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

void AuraValuesCache::RecordTooltipCall(CGUnit* unit, int32 slot)
{
	TooltipCall& pending = sAuraValuesCache.m_pending;
	pending = TooltipCall{};

	if (!unit || slot < 0 || slot >= static_cast<int32>(ClientData::Aura::MaxAuraSlots))
		return;

	ClientData::WowUnit target(unit);
	if (!target)
		return;

	pending.unitGuid = target.Guid();
	pending.slot = slot;
	pending.valid = true;
}

void AuraValuesCache::BeginTooltip(void* tooltip, uint32 spellId)
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

static void __cdecl RecordTooltipCallThunk(CGUnit* unit, int32 slot)
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
    (uint32 spellId, uint32 a2, uint32 a3, uint32 a4, uint32 a5, uint32 a6))
{
	sAuraValuesCache.BeginTooltip(self, spellId);
	CGTooltip__SetBuff(self, spellId, a2, a3, a4, a5, a6);
	sAuraValuesCache.EndTooltip();
}

const AuraValuesCache::Values* AuraValuesCache::ResolveActive(uint32 spellId)
{
	if (!m_active.valid || m_active.spellId != spellId)
		return nullptr;

	uint32 now = GetTickCount();
	Entry& entry = EntryFor(m_active.unitGuid, static_cast<uint8>(m_active.slot));
	bool stale = !entry.haveData || (now - entry.receivedMs) >= kStaleAfterMs;
	if (stale && (now - entry.lastRequestMs) >= kRequestIntervalMs)
	{
		entry.lastRequestMs = now;
		Packet(CMSG_AURA_VALUES_REQUEST).PutUInt64(m_active.unitGuid).PutUInt8(static_cast<uint8>(m_active.slot)).Send();
	}

	if (!entry.haveData || entry.values.spellId != spellId)
		return nullptr;

	return &entry.values;
}

bool AuraValuesCache::GetActiveAmount(uint32 spellId, uint32 effectIndex, int32& out)
{
	if (effectIndex < 1 || effectIndex > MaxSpellEffects)
		return false;

	const Values* values = ResolveActive(spellId);
	if (!values)
		return false;

	uint32 index = effectIndex - 1;
	if (!(values->effectMask & (1u << index)))
		return false;

	out = values->amounts[index];
	return true;
}

bool AuraValuesCache::GetActiveAbsorb(uint32 spellId, const SpellRow* spell, int32& out)
{
	const Values* values = ResolveActive(spellId);
	if (!values)
		return false;

	if (spell)
	{
		for (uint32 i = 0; i < MaxSpellEffects; ++i)
		{
			uint32 auraType = spell->m_effectAura[i];
			bool absorbs = auraType == ClientData::Aura::SPELL_AURA_SCHOOL_ABSORB || auraType == ClientData::Aura::SPELL_AURA_MANA_SHIELD;
			if (absorbs && (values->effectMask & (1u << i)))
			{
				out = values->amounts[i];
				return true;
			}
		}
	}

	for (uint32 i = 0; i < MaxSpellEffects; ++i)
	{
		if (values->effectMask & (1u << i))
		{
			out = values->amounts[i];
			return true;
		}
	}

	return false;
}

bool AuraValuesCache::GetSlotValues(uint64 guid, uint8 slot, uint32 spellId, Values& out)
{
	if (!guid || slot >= ClientData::Aura::MaxAuraSlots)
		return false;

	Entry& entry = EntryFor(guid, slot);
	if (entry.haveData && entry.values.spellId == spellId)
	{
		out = entry.values;
		return true;
	}

	uint32 shift = entry.unansweredRequests < kMaxBackoffShift ? entry.unansweredRequests : kMaxBackoffShift;
	uint32 now = GetTickCount();
	if ((now - entry.lastRequestMs) >= (kRequestIntervalMs << shift))
	{
		entry.lastRequestMs = now;
		if (entry.unansweredRequests < 0xFF)
			++entry.unansweredRequests;
		Packet(CMSG_AURA_VALUES_REQUEST).PutUInt64(guid).PutUInt8(slot).Send();
	}

	return false;
}

bool AuraValuesCache::GetActiveStacks(uint32 spellId, int32& out)
{
	const Values* values = ResolveActive(spellId);
	if (!values)
		return false;

	out = values->stacks;
	return true;
}

void AuraValuesCache::Handler_SMSG_AURA_VALUES(void*, uint32, uint32, CDataStore* pkt)
{
	Packet r(pkt);
	uint64 guid = r.GetUInt64();
	uint8 slot = r.GetUInt8();
	uint32 spellId = r.GetUInt32();
	uint8 effectMask = r.GetUInt8();
	uint8 stacks = r.GetUInt8();

	if (!guid || slot >= ClientData::Aura::MaxAuraSlots)
		return;

	Values values;
	values.spellId = spellId;
	values.effectMask = effectMask;
	values.stacks = stacks;
	for (uint32 i = 0; i < MaxSpellEffects; ++i)
	{
		if (effectMask & (1u << i))
			values.amounts[i] = r.GetInt32();
	}

	sAuraValuesCache.PruneIfLarge();

	Entry& entry = sAuraValuesCache.EntryFor(guid, slot);
	entry.values = values;
	entry.receivedMs = GetTickCount();
	entry.unansweredRequests = 0;
	entry.haveData = true;

	sUnitHealthPrediction.OnAuraValuesReceived(guid, spellId);

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

	uint32 rel = reinterpret_cast<uint32>(&CGTooltip__SetBuff_Hook) - (ClientData::Aura::SetBuffCallSite + 5);
	Util::OverwriteUInt32AtAddress(ClientData::Aura::SetBuffCallSite + 1, rel);
}
