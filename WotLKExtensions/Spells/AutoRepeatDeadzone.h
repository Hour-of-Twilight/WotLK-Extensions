#pragma once

class AutoRepeatDeadzone
{
public:
	static AutoRepeatDeadzone& Instance();

	static void Apply();

	// Percent of the default melee radius, 100 is stock and 0 disables the check.
	void SetPercent(int percent);

	int Percent() const
	{
		return m_percent;
	}

private:
	AutoRepeatDeadzone() = default;

	int m_percent = 100;
};

#define sAutoRepeatDeadzone AutoRepeatDeadzone::Instance()
