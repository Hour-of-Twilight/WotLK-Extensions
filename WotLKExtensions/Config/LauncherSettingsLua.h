#pragma once

struct lua_State;

// Launcher settings.json exposed to the options UI, at the login screen and in game.
class LauncherSettingsLua
{
public:
	static void Apply();

private:
	static int IsHDPatchEnabled(lua_State* L);
	static int SetHDPatchEnabled(lua_State* L);
	static int GetDownloadSpeedLimit(lua_State* L);
	static int SetDownloadSpeedLimit(lua_State* L);
};
