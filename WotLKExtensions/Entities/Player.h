#pragma once

#include <SharedDefines.h>
#include <unordered_map>
#include <vector>

constexpr uint32 C_MAX_SPELL_MOD_FAMILY = 32;

enum SpellModOp : uint8
{
	SPELLMOD_DAMAGE = 0,
	SPELLMOD_DURATION = 1,
	SPELLMOD_THREAT = 2,
	SPELLMOD_EFFECT1 = 3,
	SPELLMOD_CHARGES = 4,
	SPELLMOD_RANGE = 5,
	SPELLMOD_RADIUS = 6,
	SPELLMOD_CRITICAL_CHANCE = 7,
	SPELLMOD_ALL_EFFECTS = 8,
	SPELLMOD_NOT_LOSE_CASTING_TIME = 9,
	SPELLMOD_CASTING_TIME = 10,
	SPELLMOD_COOLDOWN = 11,
	SPELLMOD_EFFECT2 = 12,
	SPELLMOD_IGNORE_ARMOR = 13,
	SPELLMOD_COST = 14,
	SPELLMOD_CRIT_DAMAGE_BONUS = 15,
	SPELLMOD_RESIST_MISS_CHANCE = 16,
	SPELLMOD_JUMP_TARGETS = 17,
	SPELLMOD_CHANCE_OF_SUCCESS = 18,
	SPELLMOD_ACTIVATION_TIME = 19,
	SPELLMOD_DAMAGE_MULTIPLIER = 20,
	SPELLMOD_GLOBAL_COOLDOWN = 21,
	SPELLMOD_DOT = 22,
	SPELLMOD_EFFECT3 = 23,
	SPELLMOD_BONUS_MULTIPLIER = 24,
	// 25 unused
	SPELLMOD_PROC_PER_MINUTE = 26,
	SPELLMOD_VALUE_MULTIPLIER = 27,
	SPELLMOD_RESIST_DISPEL_CHANCE = 28,
	SPELLMOD_CRIT_DAMAGE_BONUS_2 = 29,
	SPELLMOD_SPELL_COST_REFUND_ON_FAIL = 30,

	MAX_SPELLMOD,

	SPELLMOD_CAST_WHILE_MOVING = MAX_SPELLMOD,
	SPELLMOD_POWER_TYPE,

	MAX_CUSTOM_SPELLMOD
};

class Player
{
public:
	static Player& Instance();

	int8 GetSecurityLevel() const
	{
		return m_securityLevel;
	}
	uint32 GetMagicFind() const
	{
		return m_magicFind;
	}
	float GetHealthLeech() const
	{
		return m_healthLeech;
	}
	float GetManaLeech() const
	{
		return m_manaLeech;
	}
	float GetCritDamageMod() const
	{
		return m_critDamageMod;
	}
	float GetCritHealingMod() const
	{
		return m_critHealingMod;
	}
	uint8 GetCharCreateGameMode() const
	{
		return m_charCreateGameMode;
	}
	bool IsInWorld() const
	{
		return m_playerInWorld;
	}
	uint32 GetCustomSpellPen(uint8 school) const
	{
		return m_customSpellPen[school];
	}

	void SetSecurityLevel(int8 level)
	{
		m_securityLevel = level;
	}
	void SetMagicFind(uint32 val)
	{
		m_magicFind = val;
	}
	void SetHealthLeech(float val)
	{
		m_healthLeech = val;
	}
	void SetManaLeech(float val)
	{
		m_manaLeech = val;
	}
	void SetCritDamageMod(float val)
	{
		m_critDamageMod = val;
	}
	void SetCritHealingMod(float val)
	{
		m_critHealingMod = val;
	}
	void SetCharCreateGameMode(uint8 mode)
	{
		m_charCreateGameMode = mode;
	}
	void SetInWorld(bool val)
	{
		m_playerInWorld = val;
	}
	void SetCustomSpellPen(uint8 school, uint32 val)
	{
		m_customSpellPen[school] = val;
	}

	void ApplyPatches();
	void ResetVariables();

	void SetSpellMod(uint8 family, SpellModOp op, uint8 bit, bool isPct, int32 value);
	void ClearSpellMods();
	void GetSpellModifiers(SpellRow* spell, SpellModOp op, int32& outFlat, int32& outPct) const;
	bool CanCastWhileMoving(SpellRow* spell) const;

	void RegisterCustomLuaFunctions();
	static int GetSecurityLevelFunction(lua_State* L);
	Player(const Player&) = delete;
	Player& operator=(const Player&) = delete;

private:
	Player() = default;

	int8 m_securityLevel = 0;
	uint32 m_customSpellPen[MAX_SPELL_SCHOOL] = { 0 };
	uint32 m_magicFind = 0;
	float m_healthLeech = 0.0f;
	float m_manaLeech = 0.0f;
	float m_critDamageMod = 0.0f;
	float m_critHealingMod = 0.0f;
	uint8 m_charCreateGameMode = 0;
	bool m_playerInWorld = false;

	struct SpellModEntry
	{
		int32 flat = 0;
		int32 pct = 0;
	};
	static uint32 SpellModKey(uint8 family, uint8 op, uint8 bit);
	void AccumulateBlock(uint8 family, const uint32* classMask, SpellModOp op, int32& flat, int32& pct) const;
	bool BlockHasMod(uint8 family, const uint32* classMask, SpellModOp op) const;
	std::unordered_map<uint32, SpellModEntry> m_spellMods;

	static inline uint32 memoryTable[64] = { 0 };
	static inline uint32 raceNameTable[32] = { 0 };

	static void CharacterCreationRaceCrashfix();
};

#define sPlayer Player::Instance()

__forceinline bool HasSecurityClearance(int8_t level)
{
	return sPlayer.GetSecurityLevel() >= level;
}
