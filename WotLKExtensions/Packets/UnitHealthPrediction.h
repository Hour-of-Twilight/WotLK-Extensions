#pragma once

#include <Defines.h>

#include <SharedDefines.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

class UnitHealthPrediction
{
public:
	static UnitHealthPrediction& Instance();

	uint32 GetTotalAbsorbs(uint64 guid);
	uint32 GetTotalHealAbsorbs(uint64 guid);
	uint32 GetIncomingHeals(uint64 guid, uint64 healerGuid = 0);

	void OnAuraValuesReceived(uint64 guid, uint32 spellId);

	void ClearAll();
	void Apply();
	void RegisterLuaFunctions();

	UnitHealthPrediction(const UnitHealthPrediction&) = delete;
	UnitHealthPrediction& operator=(const UnitHealthPrediction&) = delete;

private:
	UnitHealthPrediction() = default;

	struct AbsorbEffects
	{
		uint8 absorbMask = 0;
		uint8 healAbsorbMask = 0;
	};

	struct IncomingHeal
	{
		uint64 caster = 0;
		uint32 spellId = 0;
		uint32 amount = 0;
		uint32 expiresMs = 0;
	};

	const AbsorbEffects& EffectsFor(uint32 spellId);
	uint32 SumAbsorbs(uint64 guid, bool healAbsorb);
	static void SignalUnitEvent(const char* eventName, uint64 guid);

	static void Handler_SMSG_UNIT_HEAL_PREDICTION(void* param, uint32 opcode, uint32 a2, CDataStore* pkt);

	static int Script_UnitGetTotalAbsorbs(lua_State* L);
	static int Script_UnitGetTotalHealAbsorbs(lua_State* L);
	static int Script_UnitGetIncomingHeals(lua_State* L);

	std::unordered_map<uint32, AbsorbEffects> m_spellEffects;
	std::unordered_map<uint64, std::vector<IncomingHeal>> m_incomingHeals;
};

#define sUnitHealthPrediction UnitHealthPrediction::Instance()
