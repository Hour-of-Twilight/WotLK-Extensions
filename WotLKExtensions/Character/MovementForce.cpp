#include "MovementForce.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <SharedDefines.h>
#include <ClientDetours.h>
#include <ClientData/MathTypes.h>
#include <vector>
#include <cmath>
#include <cstdio>
#include <Windows.h>

using ClientData::C3Vector;

CLIENT_FUNCTION(World__Intersect, 0x0077F310, __cdecl, bool,
    (C3Vector * start, C3Vector* end, C3Vector* outHit, float* pT, uint32_t flags, int zero))

static const uint32_t kSolidWorldFlags = 0x00100111;

CLIENT_FUNCTION(CGUnit_C__SendMovementUpdate, 0x0071F0C0, __thiscall, void,
    (void* self, uint32_t timeMs, uint32_t opcode, float unused, int a4, int a5, int a6, int a7))

static const uint32_t kMsgMoveHeartbeat = 0xEE;
static const uint32_t kHeartbeatIntervalMs = 250;

struct ForceField
{
	uint32_t id;
	float originX;
	float originY;
	float originZ;
	float maxForce; // peak push speed in yards/sec, reached at the origin
	float radius;   // distance at which the push fades to zero
};

static std::vector<ForceField> g_fields;

static const uint32_t kMovementOffset = 0x788;
static const uint32_t kPosOffset = 0x10;    // C3Vector, current position
static const uint32_t kAnchorOffset = 0x4C; // C3Vector, current move-segment anchor
static const uint32_t kFlagsOffset = 0x44;  // movement flags dword

static const uint32_t kFlagSwimming = 0x00200000;
static const uint32_t kFlagFlying = 0x02000000;
static const uint32_t kFlagOnTransport = 0x00000200;

static const uint32_t kClientOwnsZ =
    0x00C010FF | kFlagSwimming | kFlagFlying | kFlagOnTransport;

static const float kWaistTrace = 1.2f;  // height above feet to sweep for walls
static const float kWallSkin = 0.1f;    // stop this far short of a wall
static const float kGroundUp = 2.0f;    // start the ground probe this far above feet
static const float kGroundDown = 3.0f;  // probe this far below feet
static const float kMaxStepDown = 2.0f; // snap down at most this far; a bigger drop is a fall
static const float kZDeadzone = 0.05f;  // ignore ground differences smaller than this

static void ApplyDrift()
{
	if (g_fields.empty())
		return;

	CGPlayer* player = ClntObjMgr::GetActivePlayerObj();
	if (!player)
		return;

	uint8_t* cmov = reinterpret_cast<uint8_t*>(player) + kMovementOffset;
	float* pos = reinterpret_cast<float*>(cmov + kPosOffset);
	float* anchor = reinterpret_cast<float*>(cmov + kAnchorOffset);

	static LARGE_INTEGER s_freq = {};
	static LARGE_INTEGER s_last = {};
	if (s_freq.QuadPart == 0)
		QueryPerformanceFrequency(&s_freq);
	LARGE_INTEGER nowQpc;
	QueryPerformanceCounter(&nowQpc);
	if (s_last.QuadPart == 0)
	{
		s_last = nowQpc;
		return;
	}
	float dt = (float)(nowQpc.QuadPart - s_last.QuadPart) / (float)s_freq.QuadPart;
	s_last = nowQpc;
	if (dt <= 0.0f)
		return;
	if (dt > 0.1f)
		dt = 0.1f; // clamp after a stall so we do not teleport

	float posX = pos[0];
	float posY = pos[1];

	float pushX = 0.0f;
	float pushY = 0.0f;

	for (const ForceField& f : g_fields)
	{
		float dx = posX - f.originX;
		float dy = posY - f.originY;
		float dist = std::sqrt(dx * dx + dy * dy);

		float dirX;
		float dirY;
		float strength;
		if (dist < 0.05f)
		{
			// Sitting on the origin: push along current facing so we still slide off.
			float facing = *reinterpret_cast<float*>(cmov + 0x20);
			dirX = std::cos(facing);
			dirY = std::sin(facing);
			strength = f.maxForce;
		}
		else
		{
			dirX = dx / dist;
			dirY = dy / dist;
			strength = f.maxForce * (1.0f - dist / f.radius);
			if (strength <= 0.0f)
				continue; // beyond the field radius
		}

		pushX += dirX * strength;
		pushY += dirY * strength;
	}

	if (pushX == 0.0f && pushY == 0.0f)
		return;

	float posZ = pos[2];
	uint32_t flags = *reinterpret_cast<uint32_t*>(cmov + kFlagsOffset);
	bool onTransport = (flags & kFlagOnTransport) != 0;

	float dX = pushX * dt;
	float dY = pushY * dt;

	if (!onTransport)
	{
		C3Vector start = { posX, posY, posZ + kWaistTrace };
		C3Vector end = { posX + dX, posY + dY, posZ + kWaistTrace };
		C3Vector hit = {};
		float t = 1.0f;
		if (World__Intersect(&start, &end, &hit, &t, kSolidWorldFlags, 0) && t < 1.0f)
		{
			float len = std::sqrt(dX * dX + dY * dY);
			float tSafe = t - (len > 0.0f ? kWallSkin / len : 0.0f);
			if (tSafe < 0.0f)
				tSafe = 0.0f;
			dX *= tSafe;
			dY *= tSafe;
		}
	}

	pos[0] = posX + dX;
	pos[1] = posY + dY;
	anchor[0] += dX;
	anchor[1] += dY;

	if ((flags & kClientOwnsZ) == 0)
	{
		C3Vector start = { pos[0], pos[1], posZ + kGroundUp };
		C3Vector end = { pos[0], pos[1], posZ - kGroundDown };
		C3Vector hit = {};
		float t = 1.0f;
		if (World__Intersect(&start, &end, &hit, &t, kSolidWorldFlags, 0))
		{
			float drop = posZ - hit.z; // positive when the ground is below the feet
			if (std::fabs(drop) > kZDeadzone && drop <= kMaxStepDown)
			{
				pos[2] = hit.z;
				anchor[2] += hit.z - posZ;
			}
		}
	}

	static uint32_t s_lastHeartbeat = 0;
	uint32_t nowMs = (uint32_t)OsGetAsyncTimeMs();
	if (nowMs - s_lastHeartbeat >= kHeartbeatIntervalMs)
	{
		s_lastHeartbeat = nowMs;
		CGUnit_C__SendMovementUpdate(player, nowMs, kMsgMoveHeartbeat, 0.0f, 0, 0, 0, 0xFF);
	}
}

CLIENT_DETOUR_THISCALL(CMovement_C__ExecuteMovement, 0x006F09F0, int, (int t0, int t1))
{
	int result = CMovement_C__ExecuteMovement(self, t0, t1);

	CGPlayer* player = ClntObjMgr::GetActivePlayerObj();
	if (player && self == reinterpret_cast<uint8_t*>(player) + kMovementOffset)
		ApplyDrift();

	return result;
}

void MovementForce::Tick()
{
	ApplyDrift();
}

MovementForce& MovementForce::Instance()
{
	static MovementForce instance;
	return instance;
}

void MovementForce::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_APPLY_MOVEMENT_FORCE, &Handler_ApplyForce);
	sCustomPacket.RegisterHandler(SMSG_CLEAR_MOVEMENT_FORCE, &Handler_ClearForce);
}

void MovementForce::Handler_ApplyForce(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint32_t id = r.GetUInt32();
	float originX = r.GetFloat();
	float originY = r.GetFloat();
	float originZ = r.GetFloat();
	float maxForce = r.GetFloat();
	float radius = r.GetFloat();
	if (radius <= 0.0f)
		radius = 1.0f;

	for (ForceField& f : g_fields)
	{
		if (f.id == id)
		{
			f.originX = originX;
			f.originY = originY;
			f.originZ = originZ;
			f.maxForce = maxForce;
			f.radius = radius;
			return;
		}
	}

	g_fields.push_back({ id, originX, originY, originZ, maxForce, radius });
}

void MovementForce::Handler_ClearForce(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	uint32_t id = Packet(pkt).GetUInt32();
	if (id == 0xFFFFFFFF)
	{
		g_fields.clear();
		return;
	}

	for (auto it = g_fields.begin(); it != g_fields.end(); ++it)
	{
		if (it->id == id)
		{
			g_fields.erase(it);
			return;
		}
	}
}
