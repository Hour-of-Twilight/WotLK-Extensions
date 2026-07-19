#include <Input/ActionRepeat.h>

#include <ClientDetours.h>
#include <ClientData/ActionBar.h>
#include <ClientData/Bindings.h>
#include <ClientData/Event.h>
#include <SharedDefines.h>
#include <Logger.h>

#include <cstdlib>
#include <cstring>

using ClientData::EVENT_ID_FOCUS;
using ClientData::EventRegisterEx;

namespace AB = ClientData::ActionBar;
namespace KB = ClientData::Bindings;

static constexpr uint32_t kCastableGraceMs = 100;

// ---- Helpers ---------------------------------------------------------------

static uint32_t NowMs()
{
	return reinterpret_cast<uint32_t(__cdecl*)()>(OS_GET_TIME_ADDR)();
}

static bool IsCasting()
{
	return *spellCast != nullptr;
}

static int ResolveMainBarSlot(int buttonIndex)
{
	int page = (*AB::s_tempPageActiveFlags != 0) ? 1 : (*AB::s_currentPage + 1);
	if (page == 1 && *AB::s_bonusBarOffset != 0)
		page = AB::kNumPages + *AB::s_bonusBarOffset;

	return (buttonIndex - 1) + (page - 1) * AB::kNumButtons;
}

static int ResolveActionSlot(const char* cmd)
{
	if (!cmd)
		return -1;

	if (_strnicmp(cmd, "ACTIONBUTTON", 12) == 0)
	{
		int idx = atoi(cmd + 12);
		if (idx >= 1 && idx <= AB::kNumButtons)
			return ResolveMainBarSlot(idx);
	}

	return -1;
}

static const char* ResolveActionCommand(void* self, void* slot, int keyMode)
{
	char buf[0x100];
	buf[0] = '\0';
	void* entry = KB::GetReducedKeyBinding(self, keyMode, slot, buf, 0x80);
	if (!entry)
		return nullptr;
	return KB::GetCommandForBinding(self, entry, keyMode);
}

static int ReadCooldownStart(int slot)
{
	int start = 0, duration = 0, enable = 0;
	AB::GetCooldown(slot, &start, &duration, &enable);
	return start;
}

CLIENT_DETOUR(AR_UseAction, AB::kUseActionAddr, __cdecl, void, (int slot, uint64_t* targetGuid, char* button))
{
	if (sActionRepeat.ConsumeSuppressUse())
		return;

	AR_UseAction(slot, targetGuid, button);
}

CLIENT_DETOUR_THISCALL(AR_ExecKey, KB::kExecKeyAddr, int, (int mods, void* slot, int isDown, int argC, int keyMode))
{
	if (sActionRepeat.Enabled())
	{
		int actionSlot = ResolveActionSlot(ResolveActionCommand(self, slot, keyMode));
		if (actionSlot >= 0)
		{
			if (isDown)
			{
				sActionRepeat.OnBindingDown(slot, actionSlot);
			}
			else if (sActionRepeat.OnBindingUp(slot))
			{
				sActionRepeat.BeginSuppressUse();
				int r = AR_ExecKey(self, mods, slot, isDown, argC, keyMode);
				sActionRepeat.EndSuppressUse();
				return r;
			}
		}
	}

	return AR_ExecKey(self, mods, slot, isDown, argC, keyMode);
}

static int32_t __cdecl AR_OnFocus(const void*, void*)
{
	sActionRepeat.ClearAll();
	return 0;
}

// ---- ActionRepeat ----------------------------------------------------------

ActionRepeat& ActionRepeat::Instance()
{
	static ActionRepeat instance;
	return instance;
}

void ActionRepeat::SetMode(int mode)
{
	if (mode < ModeOff || mode > ModeContinuous)
		mode = ModeOff;
	m_mode = mode;
	LOG_INFO << "[ActionRepeat] mode set to " << m_mode;
	if (m_mode == ModeOff)
		ClearAll();
}

void ActionRepeat::SetDelayMs(int ms)
{
	if (ms < 0)
		ms = 0;
	m_delayMs = static_cast<uint32_t>(ms);
}

void ActionRepeat::SetIntervalMs(int ms)
{
	if (ms < 1)
		ms = 1;
	m_intervalMs = static_cast<uint32_t>(ms);
}

ActionRepeat::Held* ActionRepeat::Find(void* keySlot)
{
	for (Held& e : m_held)
	{
		if (e.active && e.keySlot == keySlot)
			return &e;
	}
	return nullptr;
}

ActionRepeat::Held* ActionRepeat::Allocate(void* keySlot)
{
	if (Held* existing = Find(keySlot))
		return existing;

	for (Held& e : m_held)
	{
		if (!e.active)
			return &e;
	}
	return nullptr;
}

void ActionRepeat::OnBindingDown(void* keySlot, int actionSlot)
{
	if (m_mode == ModeOff)
		return;

	Held* e = Allocate(keySlot);
	if (!e)
		return;

	e->keySlot = keySlot;
	e->actionSlot = actionSlot;
	e->downTimeMs = NowMs();
	e->lastRepeatMs = e->downTimeMs;
	e->readySinceMs = 0;
	e->cooldownStartAtPress = ReadCooldownStart(actionSlot);
	e->fired = false;
	e->done = false;
	e->active = true;

	EnsureEventRegistered();
}

bool ActionRepeat::OnBindingUp(void* keySlot)
{
	Held* e = Find(keySlot);
	if (!e)
		return false;

	bool fired = e->fired;
	e->active = false;
	return fired;
}

void ActionRepeat::OnUpdate()
{
	if (m_mode == ModeOff)
		return;

	uint32_t now = NowMs();

	for (Held& e : m_held)
	{
		if (!e.active)
			continue;
		if (m_mode == ModeUntilCast && e.done)
			continue;
		if (now - e.downTimeMs < m_delayMs)
			continue;

		if (m_mode == ModeUntilCast)
		{
			if (ReadCooldownStart(e.actionSlot) > e.cooldownStartAtPress || IsCasting())
			{
				e.done = true;
				continue;
			}
		}

		if (IsCasting())
		{
			e.readySinceMs = 0;
			continue;
		}

		int start = 0, duration = 0, enable = 0;
		AB::GetCooldown(e.actionSlot, &start, &duration, &enable);
		bool onCooldown = duration > 0 && (now - static_cast<uint32_t>(start)) < static_cast<uint32_t>(duration);
		if (onCooldown)
		{
			e.readySinceMs = 0;
			continue;
		}
		if (e.readySinceMs == 0)
			e.readySinceMs = now;
		if (now - e.readySinceMs < kCastableGraceMs)
			continue;

		if (now - e.lastRepeatMs < m_intervalMs)
			continue;

		uint64_t guid = 0;
		char button[1] = { '\0' };
		AR_UseAction(e.actionSlot, &guid, button);
		e.lastRepeatMs = now;
		e.fired = true;
	}
}

void ActionRepeat::ClearAll()
{
	for (Held& e : m_held)
		e.active = false;
}

void ActionRepeat::EnsureEventRegistered()
{
	if (m_eventRegistered)
		return;

	EventRegisterEx(EVENT_ID_FOCUS, AR_OnFocus, nullptr, 0.0f);
	m_eventRegistered = true;
}
