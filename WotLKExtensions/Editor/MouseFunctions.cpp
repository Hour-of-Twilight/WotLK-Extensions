#include <Windows.h>
#include <ClientDetours.h>
#include <ClientData/ClientFunctions.h>
#include "Player.h"
#include <Editor/EditorObject.h>
#include <Editor/EditorRuntime.h>
#include <Editor/MouseFunctions.h>
#include <Editor/QuatFunctions.h>
#include <ClientData/Enums.h>
#include <Logger.h>
#include <cstdint>
#include <fstream>
#include <string>

using namespace ClientData;

struct HitTestResult
{
	uint32_t guidLow;  // 0x00
	uint32_t guidHigh; // 0x04
	float x;           // 0x08
	float y;           // 0x0C
	float z;           // 0x10
	float dist;        // 0x14
};

static EditorObject::DecodedGuid lastMouseGUID;
static float lastMouseHitX;
static float lastMouseHitY;
static float lastMouseHitZ;

CLIENT_DETOUR_THISCALL(CGWorldFrame__HitTestPoint, 0x004F9DA0, int, (float a2, float a3, int a4, int a5))
{
	int result = CGWorldFrame__HitTestPoint(self, a2, a3, a4, a5);

	if (a5)
	{
		HitTestResult* lastMouseHit = reinterpret_cast<HitTestResult*>(a5);
		lastMouseGUID = result >= 2
		                    ? EditorObject::DecodeClientGuid(lastMouseHit->guidLow, lastMouseHit->guidHigh)
		                    : EditorObject::DecodeClientGuid(0, 0);
		lastMouseHitX = lastMouseHit->x;
		lastMouseHitY = lastMouseHit->y;
		lastMouseHitZ = lastMouseHit->z;
	}

	return result;
}

int GetMouseWorldPosition(lua_State* L)
{
	if (!HasSecurityClearance(3))
		return 0;
	FrameScript::PushNumber(L, lastMouseHitX);
	FrameScript::PushNumber(L, lastMouseHitY);
	FrameScript::PushNumber(L, lastMouseHitZ);
	return 3;
}

int GetLastMouseoverGUID(lua_State* L)
{
	if (!HasSecurityClearance(3))
		return 0;
	FrameScript::PushString(L, EditorObject::GuidToString(lastMouseGUID.full).c_str());
	FrameScript::PushNumber(L, lastMouseGUID.entry);
	FrameScript::PushNumber(L, lastMouseGUID.low);
	FrameScript::PushNumber(L, lastMouseGUID.type);
	return 4;
}

static CGGameObject_C* SelectedGameObject()
{
	return EditorRuntime::SelectedGameObject();
}

static CGGameObject_C* GameObjectByLuaGuid(lua_State* L, int index)
{
	return EditorObject::GameObjectByGuid(std::stoull(FrameScript::ToLString(L, index, false)));
}

static CGGameObject_C* GameObjectByMouse()
{
	return EditorObject::GameObjectByGuid(lastMouseGUID.full);
}

static void PushGameObjectPosition(lua_State* L, CGGameObject_C* gameObject)
{
	FrameScript::PushNumber(L, gameObject->m_passenger.position.x);
	FrameScript::PushNumber(L, gameObject->m_passenger.position.y);
	FrameScript::PushNumber(L, gameObject->m_passenger.position.z);
}

int LogMouseoverGobValues(lua_State*)
{
	if (!HasSecurityClearance(3))
		return 0;
	CGGameObject_C* gameObject = GameObjectByMouse();
	if (!gameObject)
	{
		LOG_DEBUG << "MouseoverGobValues: no game object moused over";
		return 0;
	}

	uint32_t mapID = *reinterpret_cast<uint32_t*>(0xBD088C);
	Quat q = unpack_quat(gameObject->m_passenger.compressedRotation);

	float roll, pitch, yaw;
	quat_to_euler(q, roll, pitch, yaw);

	std::ofstream file("gobpositions", std::ios::app);
	if (file.is_open())
	{
		file << lastMouseGUID.entry << "," << mapID << ","
		     << gameObject->m_passenger.position.x << ","
		     << gameObject->m_passenger.position.y << ","
		     << gameObject->m_passenger.position.z << ","
		     << yaw << "," << q.x << "," << q.y << "," << q.z << "," << q.w << "\n";
		file.close();
	}

	return 0;
}

int ClearSelectedGob(lua_State*)
{
	if (!HasSecurityClearance(3))
		return 0;
	EditorRuntime::ClearSelection();
	return 0;
}

int SelectEditorGobByMouse(lua_State* L)
{
	if (!HasSecurityClearance(3))
		return 0;
	CGGameObject_C* gameObject = GameObjectByMouse();
	if (!gameObject || !EditorRuntime::SelectGameObject(lastMouseGUID.full))
		return 0;

	FrameScript::PushString(L, EditorObject::GuidToString(lastMouseGUID.full).c_str());
	PushGameObjectPosition(L, gameObject);
	return 4;
}

int GetSelectedGobGUID(lua_State* L)
{
	if (!HasSecurityClearance(3))
		return 0;
	FrameScript::PushString(L, EditorObject::GuidToString(EditorRuntime::CurrentSelectedGobGUID()).c_str());
	return 1;
}

int GetSelectedGobPosition(lua_State* L)
{
	if (!HasSecurityClearance(3))
		return 0;
	CGGameObject_C* gameObject = SelectedGameObject();
	if (!gameObject)
		return 0;

	PushGameObjectPosition(L, gameObject);
	return 3;
}

int GetSelectedGobRotation(lua_State* L)
{
	if (!HasSecurityClearance(3))
		return 0;
	CGGameObject_C* gameObject = SelectedGameObject();
	if (!gameObject)
		return 0;

	Quat q = unpack_quat(gameObject->m_passenger.compressedRotation);

	float roll, pitch, yaw;
	quat_to_euler(q, roll, pitch, yaw);

	FrameScript::PushNumber(L, q.x);
	FrameScript::PushNumber(L, q.y);
	FrameScript::PushNumber(L, q.z);
	FrameScript::PushNumber(L, q.w);
	return 4;
}

int SetGobPositionByGUID(lua_State* L)
{
	if (!HasSecurityClearance(3))
	{
		FrameScript::PushBoolean(L, false);
		return 1;
	}
	CGGameObject_C* gameObject = GameObjectByLuaGuid(L, 1);
	if (!gameObject)
	{
		FrameScript::PushBoolean(L, false);
		return 1;
	}

	gameObject->m_passenger.position.x = (float)FrameScript::GetNumber(L, 2);
	gameObject->m_passenger.position.y = (float)FrameScript::GetNumber(L, 3);
	gameObject->m_passenger.position.z = (float)FrameScript::GetNumber(L, 4);
	gameObject->UpdateWorldObject(0);

	FrameScript::PushBoolean(L, true);
	return 1;
}

int SetGobRotationByGUID(lua_State* L)
{
	if (!HasSecurityClearance(3))
	{
		FrameScript::PushBoolean(L, false);
		return 1;
	}
	CGGameObject_C* gameObject = GameObjectByLuaGuid(L, 1);
	if (!gameObject)
	{
		FrameScript::PushBoolean(L, false);
		return 1;
	}

	float x = (float)FrameScript::GetNumber(L, 2);
	float y = (float)FrameScript::GetNumber(L, 3);
	float z = (float)FrameScript::GetNumber(L, 4);
	float w = (float)FrameScript::GetNumber(L, 5);

	gameObject->m_passenger.compressedRotation = pack_quat({ w, x, y, z });
	gameObject->UpdateWorldObject(0);

	FrameScript::PushBoolean(L, true);
	return 1;
}

int CommitEditorGobToServer(lua_State*)
{
	if (!HasSecurityClearance(3))
		return 0;
	CGGameObject_C* gameObject = SelectedGameObject();
	if (!gameObject)
		return 0;

	uint64_t guid = EditorRuntime::CurrentSelectedGobGUID();
	float x = gameObject->m_passenger.position.x;
	float y = gameObject->m_passenger.position.y;
	float z = gameObject->m_passenger.position.z;
	Quat q = unpack_quat(gameObject->m_passenger.compressedRotation);

	CDataStore pkt;
	CDataStore_C::GenPacket(&pkt);
	CDataStore_C::PutInt32(&pkt, CMSG_GOB_EDITOR_UPDATE);
	CDataStore_C::PutInt64(&pkt, static_cast<int64_t>(guid));
	CDataStore_C::PutFloat(&pkt, x);
	CDataStore_C::PutFloat(&pkt, y);
	CDataStore_C::PutFloat(&pkt, z);
	CDataStore_C::PutFloat(&pkt, q.x);
	CDataStore_C::PutFloat(&pkt, q.y);
	CDataStore_C::PutFloat(&pkt, q.z);
	CDataStore_C::PutFloat(&pkt, q.w);
	pkt.m_read = 0;
	ClientServices::SendPacket(&pkt);
	CDataStore_C::Release(&pkt);
	return 0;
}
