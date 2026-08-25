#pragma once

#include <ClientData/WorldFrame.h>

namespace MapEditor::Runtime
{
	void Apply();
	void OnGameClientInitialize();
	void OnGameClientDestroy();

	// Called either side of the client's CGWorldFrame::OnWorldRender. The brush decal has to be
	// set up in BeforeWorldRender because the client draws it from inside that call.
	void BeforeWorldRender(ClientData::CGWorldFrameFull* worldFrame);
	void AfterWorldRender(ClientData::CGWorldFrameFull* worldFrame);
}
