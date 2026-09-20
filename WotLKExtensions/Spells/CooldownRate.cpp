#include "CooldownRate.h"

#include <Packet.h>

#include <ClientData/ClientFunctions.h>
#include <ClientData/Enums.h>
#include <ClientData/SharedDefines.h>
#include <ClientData/Spell.h>
#include <ClientDetours.h>
#include <CustomLua.h>
#include <CustomPacket.h>

#include <cstdlib>

using ClientData::SpellCooldowns::Entry;

namespace
{
	constexpr float kMinRate = 0.02f;
	constexpr float kMaxRate = 10.0f;
	constexpr uint32_t kMaxAffectedMs = 60 * 60 * 1000;
	constexpr int32_t kMatchToleranceMs = 2;

	// CGCooldown, from CGCooldown__SetCooldown (0x005ECD70) and OnLayerUpdate (0x005EC8F0).
	constexpr uintptr_t kFrameStartMs = 0x29C;
	constexpr uintptr_t kFrameDurationMs = 0x2A0;

	uint32_t NowMs()
	{
		return static_cast<uint32_t>(OsGetAsyncTimeMs());
	}

	uint32_t* FrameStart(void* frame)
	{
		return reinterpret_cast<uint32_t*>(static_cast<char*>(frame) + kFrameStartMs);
	}

	bool Near(int32_t a, int32_t b)
	{
		return std::abs(a - b) <= kMatchToleranceMs;
	}

	bool Live(Entry* entry, uint32_t now, uint32_t start, uint32_t length)
	{
		if (!length || entry->m_onHold)
			return false;

		int32_t remaining = static_cast<int32_t>(start + length - now);
		return remaining > 0 && static_cast<uint32_t>(remaining) <= kMaxAffectedMs;
	}

}

CooldownRate& CooldownRate::Instance()
{
	static CooldownRate instance;
	return instance;
}

void CooldownRate::SetRate(float rate, bool includeGcd)
{
	if (!(rate > 0.0f) || rate < kMinRate)
		rate = kMinRate;
	else if (rate > kMaxRate)
		rate = kMaxRate;

	m_rate = rate;
	m_includeGcd = includeGcd;
	m_carryMs = 0.0;
	m_lastTickMs = NowMs();
}

void CooldownRate::Forget()
{
	m_rate = 1.0f;
	m_includeGcd = false;
	m_carryMs = 0.0;
	m_frameShiftMs = 0;
	m_frames.clear();
}

void CooldownRate::Tick()
{
	uint32_t now = NowMs();
	int32_t elapsed = static_cast<int32_t>(now - m_lastTickMs);
	m_lastTickMs = now;

	m_frameShiftMs = 0;
	if (m_rate == 1.0f || elapsed <= 0 || elapsed > 5000)
		return;

	double want = static_cast<double>(elapsed) * (1.0 - m_rate) + m_carryMs;
	int32_t shift = static_cast<int32_t>(want);
	m_carryMs = want - shift;
	if (!shift)
		return;

	m_frameShiftMs = shift;

	for (Entry* entry = ClientData::SpellCooldowns::First(ClientData::SpellCooldowns::PlayerHistory); entry;
	     entry = ClientData::SpellCooldowns::Next(entry))
	{
		uint32_t gcdEnd = entry->m_startTime + entry->m_gcdDuration;
		bool gcdLive = Live(entry, now, entry->m_startTime, entry->m_gcdDuration);

		if (Live(entry, now, entry->m_startTime, entry->m_duration))
			entry->m_startTime += shift;
		else if (gcdLive && m_includeGcd)
			entry->m_startTime += shift;

		if (Live(entry, now, entry->m_categoryStart, entry->m_categoryDuration))
			entry->m_categoryStart += shift;

		if (gcdLive)
		{
			uint32_t wanted = m_includeGcd ? gcdEnd + shift : gcdEnd;
			int32_t length = static_cast<int32_t>(wanted - entry->m_startTime);
			entry->m_gcdDuration = length > 0 ? static_cast<uint32_t>(length) : 0;
		}
	}
}

bool CooldownRate::Match(int32_t startMs, int32_t durationMs, Tracked& out) const
{
	uint32_t now = NowMs();

	for (Entry* entry = ClientData::SpellCooldowns::First(ClientData::SpellCooldowns::PlayerHistory); entry;
	     entry = ClientData::SpellCooldowns::Next(entry))
	{
		if (Live(entry, now, entry->m_startTime, entry->m_duration) && Near(startMs, entry->m_startTime)
		    && Near(durationMs, entry->m_duration))
		{
			out = { entry->m_spellId, entry->m_itemId, FieldSpell };
			return true;
		}

		if (Live(entry, now, entry->m_categoryStart, entry->m_categoryDuration) && Near(startMs, entry->m_categoryStart)
		    && Near(durationMs, entry->m_categoryDuration))
		{
			out = { entry->m_spellId, entry->m_itemId, FieldCategory };
			return true;
		}

		if (m_includeGcd && Live(entry, now, entry->m_startTime, entry->m_gcdDuration)
		    && Near(startMs, entry->m_startTime) && Near(durationMs, entry->m_gcdDuration))
		{
			out = { entry->m_spellId, entry->m_itemId, FieldGcd };
			return true;
		}
	}

	return false;
}

void CooldownRate::OnCooldownFrameSet(void* frame, int32_t startMs, int32_t durationMs)
{
	Tracked tracked;
	if (Match(startMs, durationMs, tracked))
		m_frames[frame] = tracked;
	else
		m_frames.erase(frame);
}

void CooldownRate::OnCooldownFrameUpdate(void* frame)
{
	auto tracked = m_frames.find(frame);
	if (tracked == m_frames.end())
		return;

	uint32_t now = NowMs();

	for (Entry* entry = ClientData::SpellCooldowns::First(ClientData::SpellCooldowns::PlayerHistory); entry;
	     entry = ClientData::SpellCooldowns::Next(entry))
	{
		if (entry->m_spellId != tracked->second.spellId || entry->m_itemId != tracked->second.itemId)
			continue;

		uint32_t start = entry->m_startTime;
		uint32_t length = entry->m_duration;
		if (tracked->second.field == FieldCategory)
		{
			start = entry->m_categoryStart;
			length = entry->m_categoryDuration;
		}
		else if (tracked->second.field == FieldGcd)
			length = entry->m_gcdDuration;

		if (!Live(entry, now, start, length))
			continue;

		*FrameStart(frame) = start;
		return;
	}

	m_frames.erase(tracked);
}

void CooldownRate::Handler_SMSG_COOLDOWN_RATE(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	float rate = r.GetFloat();
	uint8_t flags = r.GetUInt8();

	sCooldownRate.SetRate(rate, (flags & 0x1) != 0);
}

int CooldownRate::Script_GetCooldownRate(lua_State* L)
{
	FrameScript::PushNumber(L, sCooldownRate.Rate());
	FrameScript::PushBoolean(L, sCooldownRate.AffectsGlobalCooldown() ? 1 : 0);
	return 2;
}

void CooldownRate::RegisterLuaFunctions()
{
	sLua.RegisterFunction("GetCooldownRate", &Script_GetCooldownRate, LuaFunctionState::FRAME);
}

void CooldownRate::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_COOLDOWN_RATE, &Handler_SMSG_COOLDOWN_RATE);
}

CLIENT_DETOUR(Spell_C_ClearCooldowns, 0x00804AF0, __cdecl, void, ())
{
	Spell_C_ClearCooldowns();
	sCooldownRate.Forget();
}

CLIENT_DETOUR_THISCALL(CGCooldown__SetCooldown, 0x005ECD70, int, (int32_t startMs, int32_t durationMs))
{
	int result = CGCooldown__SetCooldown(self, startMs, durationMs);
	sCooldownRate.OnCooldownFrameSet(self, startMs, durationMs);
	return result;
}

CLIENT_DETOUR_THISCALL(CGCooldown__OnLayerUpdate, 0x005EC8F0, int, (float elapsed))
{
	sCooldownRate.OnCooldownFrameUpdate(self);
	return CGCooldown__OnLayerUpdate(self, elapsed);
}
