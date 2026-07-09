#include "Main.h"
#ifdef ENABLE_DISCORD
#include "DiscordRPC.h"
#endif
#include "Spell.h"
#include "Tools/MpqScanner.h"
#include "Streaming/BackgroundDownloader.h"
#include "Entities/Item.h"
#include "Lua/XMLExtensions.h"
#include "Rendering/MSDF/MSDFBootstrap.h"
#include <Editor/EditorRuntime.h>
#include <Character/AnimationFixes.h>
#include <Logger.h>
void Main::OnAttach()
{
	sLog.Reset();
	Init();
	MSDFBootstrap::initialize();
	sMpqScanner.Start();
	if (!(Util::ReadIniValue("disableStartupDownloader", "0", true) == "1"))
		sBackgroundDownloader.Start();
	// Apply patches
	Misc::ApplyPatches();
	sPlayer.ApplyPatches();
	EditorRuntime::Apply();
	AnimationFixes::Apply();
	ClientDetours::Apply();
	FrameXMLExtensions::Apply();
	Spells::Apply();
	Item::Apply();
	CDBCMgr::Load();
#ifdef ENABLE_DISCORD
	bool disableDiscord = Util::IsWine() || Util::ReadIniValue("discordDisabled", "0", true) == "1";
	if (!disableDiscord)
	{
		sDiscord.Init();
		sDiscord.UpdateActivity("", "Staring at the login screen.", "icon_transparent_");
	}
#endif
}

void Main::Init()
{
	// Misc::SetYearOffsetMultiplier();

	if (customPackets)
		sCustomPacket.Apply();

	if (outOfBoundLuaFunctions || useCustomDBCFiles || customPackets)
	{
		// From AwesomeWotLK, invalid function pointer hack
		*(uint32_t*)0xD415B8 = 1;
		*(uint32_t*)0xD415BC = 0x7FFFFFFF;
	}

	if (outOfBoundLuaFunctions || customPackets)
	{
		sLua.Apply();
		GlueXML::RegisterFunctions();
	}
}

extern "C"
{
	__declspec(dllexport) void WotLKExtensionsDummy() {}
}

DWORD WINAPI DebuggerCheckThread(LPVOID)
{
	static bool displayed = false;

	if (!displayed && IsDebuggerPresent())
	{
		displayed = true;
		MessageBoxA(nullptr, "Hey stinky! You can just ask to see bits of code.", "Nerd", MB_OK | MB_ICONEXCLAMATION);
	}

	return 0;
}

bool __stdcall DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hinstDLL);
		CreateThread(nullptr, 0, [](LPVOID) -> DWORD
		{
			Main::OnAttach();
			CreateThread(nullptr, 0, DebuggerCheckThread, nullptr, 0, nullptr);
			return 0;
		}, nullptr, 0, nullptr);
	}
	return true;
}
