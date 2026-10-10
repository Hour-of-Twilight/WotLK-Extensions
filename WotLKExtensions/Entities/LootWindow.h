#pragma once

#include <Defines.h>

#include <cstdint>

class LootWindow
{
public:
	static LootWindow& Instance();

	LootWindow(const LootWindow&) = delete;
	LootWindow& operator=(const LootWindow&) = delete;

	void Apply();
	void Tick();

	void SetStaysOpenWhileMoving(bool enabled);

	void OnServerPushedLoot(uint64 guid);

private:
	LootWindow() = default;

	uint64 GetLootTargetGuid() const;

	uint64 m_pushedGuid = 0;
};

#define sLootWindow LootWindow::Instance()
