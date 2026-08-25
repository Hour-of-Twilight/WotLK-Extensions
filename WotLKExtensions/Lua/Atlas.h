#pragma once

struct lua_State;

// Native C_Texture atlas support. Installed before FrameXML.toc runs so UI files can call
// SetAtlas from _OnLoad instead of queueing until SharedXML shows up.
class LuaAtlas
{
public:
	static void Install(lua_State* L);

private:
	static int LoadAtlasData(lua_State* L);
	static int GetAtlasInfo(lua_State* L);
	static int SetAtlas(lua_State* L);
};
