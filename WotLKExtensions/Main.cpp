#include "Main.h"
#include "Spell.h"
#include "Tools/MpqScanner.h"
#include "Entities/Item.h"
#include "Lua/XMLExtensions.h"
#include "Rendering/MSDF/MSDFBootstrap.h"
#include <Editor/EditorRuntime.h>
#include <Editor/FreeCam.h>
#ifdef ENABLE_MAP_EDITOR
#include <Editor/Map/MapEditorRuntime.h>
#endif
#include <Character/AnimationFixes.h>
#include <Spells/AutoRepeatDeadzone.h>
#include <Spells/SpellDescriptionVars.h>
#include <Config/LauncherSettings.h>
#include <Config/LauncherSettingsLua.h>
#include <FeatureCvars.h>
#include <Logger.h>
#include <Macros.h>
#include <windows.h>
#include <detours.h>
void Main::OnAttach()
{
	sLog.Reset();
	sLauncherSettings.Load();
	Init();
	MSDFBootstrap::initialize();
	sMpqScanner.Start();
	// Apply patches
	Misc::ApplyPatches();
	sPlayer.ApplyPatches();
	EditorRuntime::Apply();
#ifdef ENABLE_MAP_EDITOR
	MapEditor::Runtime::Apply();
#endif
	FreeCam::Apply();
	AnimationFixes::Apply();
	ClientDetours::Apply();
	FrameXMLExtensions::Apply();
	Spells::Apply();
	AutoRepeatDeadzone::Apply();
	sSpellDescriptionVars.Apply();
	Item::Apply();
	CDBCMgr::Load();

	LauncherSettingsLua::Apply();
	FeatureCvars::Apply();
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

	extern __declspec(dllexport) const char HeyThereReverseEngineer[] =
	    "Brother, this shit is on github, https://github.com/Hour-of-Twilight/WotLK-Extensions";
}

CLIENT_FUNCTION(ConsoleDeviceInitialize, 0x0076AB80, __cdecl, int, (char* title, int multithreaded, int arg3))

static int __cdecl ConsoleDeviceInitializeBootstrap(char* title, int multithreaded, int arg3)
{
	Main::OnAttach();
	return ConsoleDeviceInitialize(title, multithreaded, arg3);
}

bool __stdcall DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hinstDLL);
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach((PVOID*)&ConsoleDeviceInitialize, ConsoleDeviceInitializeBootstrap);
		DetourTransactionCommit();
	}
	return true;
}
