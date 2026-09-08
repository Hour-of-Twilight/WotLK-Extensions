#include "ClientDetours.h"
#include <DllVersion.h>
#include <Logger.h>

#include <cstring>

CLIENT_DETOUR_THISCALL(Grunt__ClientLink__ProveVersion, 0x008CE720, void, (uint8* /*versionHash*/))
{
	// Use our own buffer, the caller's is a stack slot reused by the reconnect path.
	uint8 commit[20];
	memcpy(commit, DllVersion::CommitBytes.data(), sizeof(commit));

	LOG_DEBUG << "Sending DLL build " << DllVersion::Commit.data() << " to the authserver";

	Grunt__ClientLink__ProveVersion(self, commit);
}
