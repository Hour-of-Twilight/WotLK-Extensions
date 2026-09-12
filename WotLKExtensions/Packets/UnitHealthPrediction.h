#pragma once

#include <SharedDefines.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

class UnitHealthPrediction
{
public:
	static UnitHealthPrediction& Instance();

	uint32_t GetTotalAbsorbs(uint64_t guid);
	uint32_t GetTotalHealAbsorbs(uint64_t guid);
	uint32_t GetIncomingHeals(uint64_t guid, uint64_t healerGuid = 0);

	void OnAuraValuesReceived(uint64_t guid, uint32_t spellId);

	void ClearAll();
	void Apply();
	void RegisterLuaFunctions();

	UnitHealthPrediction(const UnitHealthPrediction&) = delete;
	UnitHealthPrediction& operator=(const UnitHealthPrediction&) = delete;

private:
	UnitHealthPrediction() = default;

	struct AbsorbEffects
	{
		uint8_t absorbMask = 0;
		uint8_t healAbsorbMask = 0;
	};

	struct IncomingHeal
	{
		uint64_t caster = 0;
		uint32_t spellId = 0;
		uint32_t amount = 0;
		uint32_t expiresMs = 0;
	};

	const AbsorbEffects& EffectsFor(uint32_t spellId);
	uint32_t SumAbsorbs(uint64_t guid, bool healAbsorb);
	static void SignalUnitEvent(const char* eventName, uint64_t guid);

	static void Handler_SMSG_UNIT_HEAL_PREDICTION(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);

	static int Script_UnitGetTotalAbsorbs(lua_State* L);
	static int Script_UnitGetTotalHealAbsorbs(lua_State* L);
	static int Script_UnitGetIncomingHeals(lua_State* L);

	std::unordered_map<uint32_t, AbsorbEffects> m_spellEffects;
	std::unordered_map<uint64_t, std::vector<IncomingHeal>> m_incomingHeals;
};

#define sUnitHealthPrediction UnitHealthPrediction::Instance()
