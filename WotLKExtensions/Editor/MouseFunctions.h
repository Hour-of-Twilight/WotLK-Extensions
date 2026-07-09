#pragma once

struct lua_State;

int GetMouseWorldPosition(lua_State* L);
int GetLastMouseoverGUID(lua_State* L);
int LogMouseoverGobValues(lua_State* L);
int SelectEditorGobByMouse(lua_State* L);
int GetSelectedGobGUID(lua_State* L);
int GetSelectedGobPosition(lua_State* L);
int GetSelectedGobRotation(lua_State* L);
int SetGobPositionByGUID(lua_State* L);
int SetGobRotationByGUID(lua_State* L);
int ClearSelectedGob(lua_State* L);
int CommitEditorGobToServer(lua_State* L);
