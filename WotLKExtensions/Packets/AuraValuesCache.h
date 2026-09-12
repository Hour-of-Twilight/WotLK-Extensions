#pragma once

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
		uint32_t spellId = 0;
		int32_t amounts[ClientData::Aura::MaxSpellEffects] = {};
		uint8_t effectMask = 0;
		uint8_t stacks = 0;
	};

	// effectIndex is 1 based to match $s1. False means the caller should fall back to base points.
	bool GetActiveAmount(uint32_t spellId, uint32_t effectIndex, int32_t& out);
	bool GetActiveAbsorb(uint32_t spellId, const SpellRow* spell, int32_t& out);
	bool GetActiveStacks(uint32_t spellId, int32_t& out);

	bool GetSlotValues(uint64_t guid, uint8_t slot, uint32_t spellId, Values& out);

	void ClearUnit(uint64_t guid);
	void ClearAll();
	void PruneIfLarge();
	void Apply();

	// Called from the naked stub on SetTooltipUnitAura's call site. Only arms the record.
	static void __cdecl RecordTooltipCall(CGUnit* unit, int32_t slot);

	// The variables resolve only between these two.
	void BeginTooltip(void* tooltip, uint32_t spellId);
	void EndTooltip();

	AuraValuesCache(const AuraValuesCache&) = delete;
	AuraValuesCache& operator=(const AuraValuesCache&) = delete;

private:
	AuraValuesCache() = default;

	struct Entry
	{
		Values values;
		uint32_t lastRequestMs = 0;
		uint32_t receivedMs = 0;
		uint8_t unansweredRequests = 0;
		bool haveData = false;
	};

	Entry& EntryFor(uint64_t guid, uint8_t slot);
	const Values* ResolveActive(uint32_t spellId);

	// SMSG_AURA_VALUES: Int64 guid, UInt8 slot, UInt32 spellId, UInt8 effectMask, UInt8 stacks,
	// Int32 amount per bit set in effectMask.
	static void Handler_SMSG_AURA_VALUES(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);

	std::unordered_map<uint64_t, std::unordered_map<uint8_t, Entry>> m_entries;

	ClientData::Aura::TooltipCall m_pending{};   // armed by the stub
	ClientData::Aura::TooltipCall m_active{};    // live only inside SetBuff
	ClientData::Aura::TooltipCall m_displayed{}; // last rendered, for the refresh event
};

#define sAuraValuesCache AuraValuesCache::Instance()
