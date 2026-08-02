#pragma once

#include <cstdint>

struct CDataStore;

class MovementForce
{
public:
	static MovementForce& Instance();

	MovementForce(const MovementForce&) = delete;
	MovementForce& operator=(const MovementForce&) = delete;

	void Apply();

	void Tick();

private:
	MovementForce() = default;

	static void Handler_ApplyForce(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_ClearForce(void*, uint32_t, uint32_t, CDataStore* pkt);
};

#define sMovementForce MovementForce::Instance()
