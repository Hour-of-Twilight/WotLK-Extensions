#include "ClientDetours.h"
#include <SharedDefines.h>
#include <Item.h>
#include <Player.h>
#include <CustomPacket.h>
#include <Logger.h>
#include <Misc.h>
#include <UnitLevelCache.h>

CLIENT_DETOUR(CGlueMgr__EnterWorld, 0x4D9BD0, __cdecl, void, ())
{
	CGlueMgr__EnterWorld();
	sPlayer.SetInWorld(true);
	sPlayer.ResetVariables();
	sCustomPacket.SendSanityCheck(true);
}

CLIENT_DETOUR(CMap__SafeOpen, 0x007BD480, __cdecl, int, (char* Src, HANDLE* a2))
{
	for (int i = 0; i < 10; i++)
	{
		if (SFile::OpenFile(Src, a2))
		{
			return 1;
		}
	}
	LOG_DEBUG << "BAD FILE READ, NO FIND " << Src;
	return SFile::OpenFile("Spells\\ErrorCube.mdx", a2);
}

CLIENT_DETOUR(InitializeProgressBar, 0x0040A990, __cdecl, int, (char arg0))
{
	int result = InitializeProgressBar(arg0);
	sUnitLevelCache.ClearCreatures();
	return result;
}

CLIENT_DETOUR(Script_CreateCharacter, 0x004E0C60, __cdecl, int, (lua_State * L))
{
	char* str = FrameScript::ToLString(L, 1, false);
	sPlayer.SetCharCreateGameMode(FrameScript::GetNumber(L, 2));
	CCharacterCreation__CreateCharacter(str);

	return 0;
}
