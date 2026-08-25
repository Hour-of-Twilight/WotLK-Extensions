#include "MovementForce.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <SharedDefines.h>
#include <ClientDetours.h>
#include <ClientData/MathTypes.h>
#include <ClientData/ObjectManager.h>
#include <vector>
#include <cmath>
#include <Windows.h>

using ClientData::C3Vector;

CLIENT_FUNCTION(World__Intersect, 0x0077F310, __cdecl, bool,
    (C3Vector * start, C3Vector* end, C3Vector* outHit, float* pT, uint32_t flags, int zero))

static const uint32_t kSolidWorldFlags = 0x00100111;

CLIENT_FUNCTION(CGUnit_C__SendMovementUpdate, 0x0071F0C0, __thiscall, void,
    (void* self, uint32_t timeMs, uint32_t opcode, float unused, int a4, int a5, int a6, int a7))

static const uint32_t kMsgMoveHeartbeat = 0xEE;

// The pushed client is still authoritative over its own position, so it has to keep telling the
// server where the drift put it. Observers fill the gaps by simulating, so this only has to be
// often enough that the two do not diverge.
static const uint32_t kHeartbeatIntervalMs = 250;

enum ForceType : uint8_t
{
	FORCE_TYPE_DIRECTIONAL = 0, // constant push along a fixed direction
	FORCE_TYPE_RADIAL = 1,      // pushes away from (or toward, if magnitude is negative) an origin
};

enum ForceFlags : uint32_t
{
	FORCE_FLAG_NO_COLLISION = 0x1, // do not sweep for walls, push straight through
	FORCE_FLAG_NO_GROUND = 0x2,    // do not snap to the ground after moving
};

struct ForceField
{
	uint32_t id;
	uint8_t type;
	C3Vector origin;
	C3Vector direction; // normalized, directional forces only
	float magnitude;    // peak push speed in yards/sec
	float radius;       // distance the push fades to zero over, 0 means no falloff
	uint32_t flags;
	int32_t durationMs; // 0 means until the server clears it
	uint32_t startMs;   // client time the force became active here
};

struct UnitForces
{
	uint64_t guid;
	std::vector<ForceField> fields;
	uint32_t lastHeartbeatMs;
};

static std::vector<UnitForces> g_units;

static const uint32_t kMovementOffset = 0x788;
static const uint32_t kPosOffset = 0x10;    // C3Vector, current position
static const uint32_t kFacingOffset = 0x20; // float, current facing
static const uint32_t kFlagsOffset = 0x44;  // movement flags dword
static const uint32_t kAnchorOffset = 0x4C; // C3Vector, current move-segment anchor

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
static const float kOriginDeadzone = 0.05f; // close enough to a radial origin to count as on it

static UnitForces* FindUnit(uint64_t guid)
{
	for (UnitForces& u : g_units)
	{
		if (u.guid == guid)
			return &u;
	}

	return nullptr;
}

// Sums every active force on a unit into a push velocity, in yards/sec. dt is only used to clamp
// an inward force to the distance it has left.
static void SumForces(const std::vector<ForceField>& fields, const float* pos, float facing, float dt,
    float& outX, float& outY, float& outZ, uint32_t& outFlags)
{
	outX = 0.0f;
	outY = 0.0f;
	outZ = 0.0f;
	outFlags = 0;

	for (const ForceField& f : fields)
	{
		if (f.type == FORCE_TYPE_DIRECTIONAL)
		{
			outX += f.direction.x * f.magnitude;
			outY += f.direction.y * f.magnitude;
			outZ += f.direction.z * f.magnitude;
			outFlags |= f.flags;
			continue;
		}

		float dx = pos[0] - f.origin.x;
		float dy = pos[1] - f.origin.y;
		float dist = std::sqrt(dx * dx + dy * dy);

		bool inward = f.magnitude < 0.0f; // dir points away from origin, a negative magnitude pulls

		float dirX;
		float dirY;
		if (dist < kOriginDeadzone)
		{
			if (inward)
				continue;

			// Sitting on the origin: push along current facing so we still slide off.
			dirX = std::cos(facing);
			dirY = std::sin(facing);
		}
		else
		{
			dirX = dx / dist;
			dirY = dy / dist;
		}

		float strength = f.magnitude;
		if (f.radius > 0.0f)
		{
			if (dist >= f.radius)
				continue; // beyond the field

			strength *= 1.0f - dist / f.radius;
		}

		// Never let a pull carry the unit past the origin in one frame.
		if (inward && dt > 0.0f && -strength * dt > dist)
			strength = -dist / dt;

		outX += dirX * strength;
		outY += dirY * strength;
		outFlags |= f.flags;
	}
}

// Moves one unit by the forces acting on it. Runs identically on every client that can see the
// unit, which is what keeps the motion in sync without a packet per frame.
static void DriftUnit(void* unit, const std::vector<ForceField>& fields, float dt)
{
	uint8_t* cmov = reinterpret_cast<uint8_t*>(unit) + kMovementOffset;
	float* pos = reinterpret_cast<float*>(cmov + kPosOffset);
	float* anchor = reinterpret_cast<float*>(cmov + kAnchorOffset);
	float facing = *reinterpret_cast<float*>(cmov + kFacingOffset);
	uint32_t moveFlags = *reinterpret_cast<uint32_t*>(cmov + kFlagsOffset);

	float pushX;
	float pushY;
	float pushZ;
	uint32_t forceFlags;
	SumForces(fields, pos, facing, dt, pushX, pushY, pushZ, forceFlags);

	if (pushX == 0.0f && pushY == 0.0f && pushZ == 0.0f)
		return;

	float posX = pos[0];
	float posY = pos[1];
	float posZ = pos[2];

	float dX = pushX * dt;
	float dY = pushY * dt;
	float dZ = pushZ * dt;

	bool onTransport = (moveFlags & kFlagOnTransport) != 0;
	if (!onTransport && (forceFlags & FORCE_FLAG_NO_COLLISION) == 0)
	{
		C3Vector start = { posX, posY, posZ + kWaistTrace };
		C3Vector end = { posX + dX, posY + dY, posZ + kWaistTrace + dZ };
		C3Vector hit = {};
		float t = 1.0f;
		if (World__Intersect(&start, &end, &hit, &t, kSolidWorldFlags, 0) && t < 1.0f)
		{
			float len = std::sqrt(dX * dX + dY * dY + dZ * dZ);
			float tSafe = t - (len > 0.0f ? kWallSkin / len : 0.0f);
			if (tSafe < 0.0f)
				tSafe = 0.0f;
			dX *= tSafe;
			dY *= tSafe;
			dZ *= tSafe;
		}
	}

	pos[0] = posX + dX;
	pos[1] = posY + dY;
	anchor[0] += dX;
	anchor[1] += dY;

	if (dZ != 0.0f)
	{
		pos[2] = posZ + dZ;
		anchor[2] += dZ;
		return;
	}

	bool snapToGround = (moveFlags & kClientOwnsZ) == 0 && (forceFlags & FORCE_FLAG_NO_GROUND) == 0;
	if (!snapToGround)
		return;

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

// Tells the server where our own drift put us. Observers already know the force and are
// simulating it, so this is a correction channel, not the thing that drives their view.
static void SendSelfHeartbeat(void* player, UnitForces& forces, uint32_t nowMs)
{
	if (nowMs - forces.lastHeartbeatMs < kHeartbeatIntervalMs)
		return;

	forces.lastHeartbeatMs = nowMs;
	CGUnit_C__SendMovementUpdate(player, nowMs, kMsgMoveHeartbeat, 0.0f, 0, 0, 0, 0xFF);
}

static void TickForces()
{
	if (g_units.empty())
		return;

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

	uint32_t nowMs = (uint32_t)OsGetAsyncTimeMs();
	uint64_t selfGuid = ClntObjMgr::GetActivePlayer();

	for (size_t i = 0; i < g_units.size();)
	{
		UnitForces& u = g_units[i];

		for (size_t f = 0; f < u.fields.size();)
		{
			const ForceField& field = u.fields[f];
			if (field.durationMs > 0 && nowMs - field.startMs >= (uint32_t)field.durationMs)
				u.fields.erase(u.fields.begin() + f);
			else
				++f;
		}

		if (u.fields.empty())
		{
			g_units.erase(g_units.begin() + i);
			continue;
		}

		void* unit = ClientData::ObjectManager::GetObject(u.guid,
		    static_cast<ClientData::ObjectTypeMask>(
		        ClientData::TYPEMASK_UNIT | ClientData::TYPEMASK_PLAYER));
		if (unit)
		{
			DriftUnit(unit, u.fields, dt);
			if (u.guid == selfGuid)
				SendSelfHeartbeat(unit, u, nowMs);
		}

		++i;
	}
}

void MovementForce::Tick()
{
	TickForces();
}

void MovementForce::Reset()
{
	g_units.clear();
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

// Confirms to the server that we started or stopped drifting, and hands it the position we were
// at when it happened so it can rebase observers on an exact value instead of the next heartbeat.
static void SendAck(uint64_t guid, uint32_t forceId, bool applied)
{
	if (guid != ClntObjMgr::GetActivePlayer())
		return;

	void* player = ClntObjMgr::GetActivePlayerObj();
	if (!player)
		return;

	uint8_t* cmov = reinterpret_cast<uint8_t*>(player) + kMovementOffset;
	const float* pos = reinterpret_cast<const float*>(cmov + kPosOffset);
	float facing = *reinterpret_cast<const float*>(cmov + kFacingOffset);

	Packet ack(CMSG_MOVEMENT_FORCE_ACK);
	ack.PutUInt64(guid);
	ack.PutUInt32(forceId);
	ack.PutUInt8(applied ? 1 : 0);
	ack.PutFloat(pos[0]);
	ack.PutFloat(pos[1]);
	ack.PutFloat(pos[2]);
	ack.PutFloat(facing);
	ack.Send();
}

void MovementForce::Handler_ApplyForce(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);

	uint64_t guid = r.GetUInt64();

	ForceField field = {};
	field.id = r.GetUInt32();
	field.type = r.GetUInt8();
	field.origin.x = r.GetFloat();
	field.origin.y = r.GetFloat();
	field.origin.z = r.GetFloat();
	field.direction.x = r.GetFloat();
	field.direction.y = r.GetFloat();
	field.direction.z = r.GetFloat();
	field.magnitude = r.GetFloat();
	field.radius = r.GetFloat();
	field.flags = r.GetUInt32();
	field.durationMs = r.GetInt32();
	field.startMs = (uint32_t)OsGetAsyncTimeMs();

	if (field.radius < 0.0f)
		field.radius = 0.0f;

	if (field.type == FORCE_TYPE_DIRECTIONAL)
	{
		float len = std::sqrt(field.direction.x * field.direction.x +
		    field.direction.y * field.direction.y + field.direction.z * field.direction.z);
		if (len <= 0.0001f)
			return; // a directional force with no direction does nothing

		field.direction.x /= len;
		field.direction.y /= len;
		field.direction.z /= len;
	}

	UnitForces* u = FindUnit(guid);
	if (!u)
	{
		g_units.push_back({ guid, {}, (uint32_t)OsGetAsyncTimeMs() });
		u = &g_units.back();
	}

	for (ForceField& existing : u->fields)
	{
		if (existing.id == field.id)
		{
			// Keep the original start so a refresh does not extend a timed force.
			field.startMs = existing.startMs;
			existing = field;
			SendAck(guid, field.id, true);
			return;
		}
	}

	u->fields.push_back(field);
	SendAck(guid, field.id, true);
}

void MovementForce::Handler_ClearForce(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint64_t guid = r.GetUInt64();
	uint32_t id = r.GetUInt32();

	for (size_t i = 0; i < g_units.size(); ++i)
	{
		if (g_units[i].guid != guid)
			continue;

		if (id == 0xFFFFFFFF)
		{
			g_units.erase(g_units.begin() + i);
			SendAck(guid, id, false);
			return;
		}

		std::vector<ForceField>& fields = g_units[i].fields;
		for (size_t f = 0; f < fields.size(); ++f)
		{
			if (fields[f].id != id)
				continue;

			fields.erase(fields.begin() + f);
			if (fields.empty())
				g_units.erase(g_units.begin() + i);

			SendAck(guid, id, false);
			return;
		}

		return;
	}
}
