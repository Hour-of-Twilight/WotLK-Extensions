#include "StartupDownloaderCvar.h"

#include <Cvars.h>
#include <Streaming/BackgroundDownloader.h>

namespace StartupDownloaderCvar
{
	void Apply()
	{
		sCvars.Register("enableStartupDownloader", "Run the background asset downloader on startup.", true,
			[](Cvar& c)
			{
				static bool started = false;
				if (c.AsBool() && !started)
				{
					started = true;
					sBackgroundDownloader.Start();
				}
			});
	}
}
