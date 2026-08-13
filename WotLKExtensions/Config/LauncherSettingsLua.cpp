#include <Config/LauncherSettingsLua.h>

#include <Config/LauncherSettings.h>
#include <Streaming/BackgroundDownloader.h>
#include <ClientData/ClientFunctions.h>
#include <CustomLua.h>

int LauncherSettingsLua::IsHDPatchEnabled(lua_State* L)
{
	FrameScript::PushBoolean(L, sLauncherSettings.HdPatch() ? 1 : 0);
	return 1;
}

int LauncherSettingsLua::SetHDPatchEnabled(lua_State* L)
{
	sLauncherSettings.SetHdPatch(FrameScript::ToBoolean(L, 1));

	sBackgroundDownloader.Trigger();
	return 0;
}

int LauncherSettingsLua::GetDownloadSpeedLimit(lua_State* L)
{
	FrameScript::PushNumber(L, sLauncherSettings.MaxDownloadMBps());
	return 1;
}

int LauncherSettingsLua::SetDownloadSpeedLimit(lua_State* L)
{
	double mbps = FrameScript::IsNumber(L, 1) ? FrameScript::GetNumber(L, 1) : 0.0;
	sLauncherSettings.SetMaxDownloadMBps(mbps);
	return 0;
}

void LauncherSettingsLua::Apply()
{
	sLua.RegisterFunction("IsHDPatchEnabled", &IsHDPatchEnabled, LuaFunctionState::ALL);
	sLua.RegisterFunction("SetHDPatchEnabled", &SetHDPatchEnabled, LuaFunctionState::ALL);
	sLua.RegisterFunction("GetDownloadSpeedLimit", &GetDownloadSpeedLimit, LuaFunctionState::ALL);
	sLua.RegisterFunction("SetDownloadSpeedLimit", &SetDownloadSpeedLimit, LuaFunctionState::ALL);
}
