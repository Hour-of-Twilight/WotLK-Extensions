#pragma once

#include <cstdint>

struct CDataStore;

// Server-replicated movement forces. The server owns the force list for a unit and pushes it to
// every client that can see that unit, so all of them run the same drift simulation locally and
// the pushed unit moves smoothly on every screen instead of snapping between heartbeats.
class MovementForce
{
public:
	static MovementForce& Instance();

	MovementForce(const MovementForce&) = delete;
	MovementForce& operator=(const MovementForce&) = delete;

	void Apply();

	void Tick();

	// Drop everything on world change, the server re-sends what is still active.
	void Reset();

private:
	MovementForce() = default;

	static void Handler_ApplyForce(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_ClearForce(void*, uint32_t, uint32_t, CDataStore* pkt);
};

#define sMovementForce MovementForce::Instance()
