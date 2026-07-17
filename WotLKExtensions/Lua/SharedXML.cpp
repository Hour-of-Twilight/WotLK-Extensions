#include <ClientDetours.h>
#include <SharedDefines.h>
#include <ClientData/ClientFunctions.h>
#include <Logger.h>
#include <cstdio>
#include <string.h>

CLIENT_DETOUR(FrameXML_CreateFrames, 0x00814340, __cdecl, int, (const char* tocPath, int flags, void* md5Ctx, void* statusObj))
{
	static const char* const kSharedToc = "Interface\\SharedXML\\SharedXML.toc";

	const char* state = nullptr;
	if (tocPath && _stricmp(tocPath, "Interface\\FrameXML\\FrameXML.toc") == 0)
		state = "frame";
	else if (tocPath && _stricmp(tocPath, "Interface\\GlueXML\\GlueXML.toc") == 0)
		state = "glue";

	int result = FrameXML_CreateFrames(tocPath, flags, md5Ctx, statusObj);

	if (state && SFile::FileExistsEx(kSharedToc, 1))
	{
		LOG_DEBUG << "Loading " << kSharedToc << " (" << state << ")";

		char marker[64];
		_snprintf(marker, sizeof(marker), "HOT_LUA_STATE=\"%s\"", state);
		FrameScript::Execute(marker, "SharedXML", 0);

		char throwawayMd5[128] = {};
		FrameXML_CreateFrames(kSharedToc, flags, throwawayMd5, statusObj);
	}

	return result;
}
