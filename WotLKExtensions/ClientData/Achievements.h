#pragma once

#include <Macros.h>

namespace ClientData
{
	namespace Achievements
	{
		CLIENT_FUNCTION(ShutdownGame, 0x005B4F00, __cdecl, void, ())
		CLIENT_FUNCTION(InitializeGame, 0x005B6DF0, __cdecl, void, ())

		inline void RebuildIndex()
		{
			ShutdownGame();
			InitializeGame();
		}

		inline bool g_rebuildPending = false;

		inline void RequestRebuild()
		{
			g_rebuildPending = true;
		}

		inline bool ConsumeRebuildRequest()
		{
			bool pending = g_rebuildPending;
			g_rebuildPending = false;
			return pending;
		}
	}
}
