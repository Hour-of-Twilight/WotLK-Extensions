#pragma once

void OnUnitPostInit(void* unit, double st0, int arg0, int arg4, int arg8);

void OnUnitPostReenable(void* unit, double st0);

int OnUnitLevelChange(void* unit);

struct lua_State;
int Script_TrueLevel(lua_State* L);
