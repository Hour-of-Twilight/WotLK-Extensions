#pragma once

#include <cstdint>

class ActionRepeat
{
public:
	enum Mode
	{
		ModeOff = 0,        // feature disabled
		ModeUntilCast = 1,  // repeat until a press actually casts, then stop
		ModeContinuous = 2, // repeat for as long as the key is held
	};

	static ActionRepeat& Instance();

	void SetMode(int mode);
	void SetDelayMs(int ms);
	void SetIntervalMs(int ms);

	bool Enabled() const
	{
		return m_mode != ModeOff;
	}

	void OnBindingDown(void* keySlot, int actionSlot);
	bool OnBindingUp(void* keySlot);

	void BeginSuppressUse()
	{
		m_suppressUse = true;
	}
	void EndSuppressUse()
	{
		m_suppressUse = false;
	}
	bool ConsumeSuppressUse()
	{
		bool suppress = m_suppressUse;
		m_suppressUse = false;
		return suppress;
	}

	void OnUpdate();
	void ClearAll();

private:
	ActionRepeat() = default;

	struct Held
	{
		void* keySlot;  // identity of the held key (ExecKey slot pointer)
		int actionSlot; // resolved 0-based action bar slot
		uint32_t downTimeMs;
		uint32_t lastRepeatMs;
		uint32_t readySinceMs;    // when the action last became castable, 0 while on cooldown
		int cooldownStartAtPress; // cooldown start stamp sampled on key-down
		bool fired;               // a repeat has fired at least once
		bool done;                // ModeUntilCast: a cast happened, stop repeating
		bool active;
	};

	Held* Find(void* keySlot);
	Held* Allocate(void* keySlot);
	void EnsureEventRegistered();

	static constexpr int kMaxHeld = 8;
	Held m_held[kMaxHeld] = {};
	int m_mode = ModeOff;
	uint32_t m_delayMs = 500;
	uint32_t m_intervalMs = 100;
	bool m_eventRegistered = false;
	bool m_suppressUse = false;
};

#define sActionRepeat ActionRepeat::Instance()
