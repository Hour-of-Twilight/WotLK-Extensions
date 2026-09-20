#pragma once

#include <cstdint>
#include <unordered_map>

struct lua_State;
struct CDataStore;

class CooldownRate
{
public:
	static CooldownRate& Instance();

	void Apply();
	void RegisterLuaFunctions();
	void Tick();

	float Rate() const
	{
		return m_rate;
	}

	bool AffectsGlobalCooldown() const
	{
		return m_includeGcd;
	}

	void SetRate(float rate, bool includeGcd);
	void Forget();

	void OnCooldownFrameSet(void* frame, int32_t startMs, int32_t durationMs);
	void OnCooldownFrameUpdate(void* frame);

	CooldownRate(const CooldownRate&) = delete;
	CooldownRate& operator=(const CooldownRate&) = delete;

private:
	CooldownRate() = default;

	enum Field : uint8_t
	{
		FieldSpell,
		FieldCategory,
		FieldGcd
	};

	struct Tracked
	{
		uint32_t spellId;
		uint32_t itemId;
		Field field;
	};

	static void Handler_SMSG_COOLDOWN_RATE(void*, uint32_t, uint32_t, CDataStore* pkt);
	static int Script_GetCooldownRate(lua_State* L);

	bool Match(int32_t startMs, int32_t durationMs, Tracked& out) const;

	float m_rate = 1.0f;
	bool m_includeGcd = false;
	double m_carryMs = 0.0;
	uint32_t m_lastTickMs = 0;
	int32_t m_frameShiftMs = 0;
	std::unordered_map<void*, Tracked> m_frames;
};

#define sCooldownRate CooldownRate::Instance()
