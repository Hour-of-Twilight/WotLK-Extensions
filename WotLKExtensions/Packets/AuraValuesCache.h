#pragma once

#include <Defines.h>

#include <ClientData/Aura.h>
#include <SharedDefines.h>

#include <cstdint>
#include <unordered_map>

// Server-side aura amounts for the aura a tooltip is rendering, feeding the ${} formula variables.
// See Docs/SpellDescriptionParsing.md.
class AuraValuesCache
{
public:
	static AuraValuesCache& Instance();

	struct Values
	{
		uint32 spellId = 0;
		int32 amounts[ClientData::Aura::MaxSpellEffects] = {};
		uint8 effectMask = 0;
		uint8 stacks = 0;
	};

	// effectIndex is 1 based to match $s1. False means the caller should fall back to base points.
	bool GetActiveAmount(uint32 spellId, uint32 effectIndex, int32& out);
	bool GetActiveAbsorb(uint32 spellId, const SpellRow* spell, int32& out);
	bool GetActiveStacks(uint32 spellId, int32& out);

	bool GetSlotValues(uint64 guid, uint8 slot, uint32 spellId, Values& out);

	void ClearUnit(uint64 guid);
	void ClearAll();
	void PruneIfLarge();
	void Apply();

	// Called from the naked stub on SetTooltipUnitAura's call site. Only arms the record.
	static void __cdecl RecordTooltipCall(CGUnit* unit, int32 slot);

	// The variables resolve only between these two.
	void BeginTooltip(void* tooltip, uint32 spellId);
	void EndTooltip();

	AuraValuesCache(const AuraValuesCache&) = delete;
	AuraValuesCache& operator=(const AuraValuesCache&) = delete;

private:
	AuraValuesCache() = default;

	struct Entry
	{
		Values values;
		uint32 lastRequestMs = 0;
		uint32 receivedMs = 0;
		uint8 unansweredRequests = 0;
		bool haveData = false;
	};

	Entry& EntryFor(uint64 guid, uint8 slot);
	const Values* ResolveActive(uint32 spellId);

	// SMSG_AURA_VALUES: Int64 guid, UInt8 slot, UInt32 spellId, UInt8 effectMask, UInt8 stacks,
	// Int32 amount per bit set in effectMask.
	static void Handler_SMSG_AURA_VALUES(void* param, uint32 opcode, uint32 a2, CDataStore* pkt);

	std::unordered_map<uint64, std::unordered_map<uint8, Entry>> m_entries;

	ClientData::Aura::TooltipCall m_pending{};   // armed by the stub
	ClientData::Aura::TooltipCall m_active{};    // live only inside SetBuff
	ClientData::Aura::TooltipCall m_displayed{}; // last rendered, for the refresh event
};

#define sAuraValuesCache AuraValuesCache::Instance()
