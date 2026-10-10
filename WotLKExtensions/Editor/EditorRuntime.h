#pragma once

#include <Defines.h>

#include <ClientData/WorldFrame.h>
#include <ClientData/GameClient.h>
#include <ClientData/GameObject.h>
#include <cstdint>

namespace EditorRuntime
{
	bool SelectGameObject(uint64 guid);
	void Apply();
	void OnGameClientInitialize();
	void OnGameClientDestroy();
	void ClearSelection();
	ClientData::CGGameObject_C* SelectedGameObject();
	uint64 CurrentSelectedGobGUID();
	void OnWorldRender(ClientData::CGWorldFrameFull* worldFrame);
}
