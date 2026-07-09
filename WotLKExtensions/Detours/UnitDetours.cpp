#include "UnitDetours.h"
#include <ClientDetours.h>
#include <ClientData/ClientFunctions.h>
#include <SharedDefines.h>
#include <UnitLevelCache.h>

using UnitCallFn = void(__cdecl*)(void);

inline UnitCallFn CGUnit_C__PostInit = reinterpret_cast<UnitCallFn>(0x0073FCC0);

__declspec(naked) static void CGUnit_C__PostInit_Detour()
{
	__asm
	    {
        push    ebp
        mov     ebp, esp
        sub     esp, 12
        push    esi
        mov     [ebp-4], ecx
        fstp    qword ptr [ebp-12]

        push    dword ptr [ebp+16]
        push    dword ptr [ebp+12]
        push    dword ptr [ebp+8]
        fld     qword ptr [ebp-12]
        mov     ecx, [ebp-4]
        call    CGUnit_C__PostInit

        mov     esi, eax

        push    dword ptr [ebp+16] // arg8
        push    dword ptr [ebp+12] // arg4
        push    dword ptr [ebp+8] // arg0
        push    dword ptr [ebp-8] // st0 hi
        push    dword ptr [ebp-12] // st0 lo
        push    dword ptr [ebp-4] // unit
        call    OnUnitPostInit
        add     esp, 24

        mov     eax, esi
        pop     esi
        mov     esp, ebp
        pop     ebp
        retn    0x0C
	    }
}

static int sPostInitReg = ClientDetours::Add(
    "CGUnit_C__PostInit",
    &CGUnit_C__PostInit,
    (void*)CGUnit_C__PostInit_Detour,
    __FILE__, __LINE__);

// 0x007402B0  __usercall(ecx, st0)  plain retn
inline UnitCallFn CGUnit_C__PostReenable = reinterpret_cast<UnitCallFn>(0x007402B0);

__declspec(naked) static void CGUnit_C__PostReenable_Detour()
{
	// After push ecx + sub esp,8:
	// [esp+0..7]=st0  [esp+8]=unit  [esp+12]=retaddr
	__asm
	{
        push    ecx
        sub     esp, 8
        fstp    qword ptr [esp]

        fld     qword ptr [esp]
        mov     ecx, [esp+8]
        call    CGUnit_C__PostReenable

        push    dword ptr [esp+8] // unit
        call    OnUnitPostReenable
        add     esp, 12

        add     esp, 4
        ret
	}
}

static int sPostReenableReg = ClientDetours::Add(
    "CGUnit_C__PostReenable",
    &CGUnit_C__PostReenable,
    (void*)CGUnit_C__PostReenable_Detour,
    __FILE__, __LINE__);

inline UnitCallFn CGUnit_C__OnLevelChange = reinterpret_cast<UnitCallFn>(0x0072E3A0);

__declspec(naked) static void CGUnit_C__OnLevelChange_Detour()
{
	__asm
	{
        push    ebp
        mov     ebp, esp
        sub     esp, 8
        push    esi
        mov     esi, ecx
        fstp    qword ptr [ebp-8]

        push    esi
        call    OnUnitLevelChange
        add     esp, 4
        test    eax, eax
        jz      skip_original

        fld     qword ptr [ebp-8]
        mov     ecx, esi
        call    CGUnit_C__OnLevelChange

    skip_original:
        pop     esi
        mov     esp, ebp
        pop     ebp
        ret
	}
}

static int sOnLevelChangeReg = ClientDetours::Add(
    "CGUnit_C__OnLevelChange",
    &CGUnit_C__OnLevelChange,
    (void*)CGUnit_C__OnLevelChange_Detour,
    __FILE__, __LINE__);

static void* GetNameplateLevelColor(int displayLevel, int playerLevel)
{
	void* result;
	__asm {
        mov edx, 0x0098E5F0
        mov ecx, displayLevel
        mov esi, playerLevel
        call edx
        mov result, eax
	}
	return result;
}

CLIENT_DETOUR(Script_UnitLevel, 0x0060F9E0, __cdecl, int, (lua_State * L))
{
	const char* token = FrameScript::ToLString(L, 1, false);
	if (token)
	{
		uint64_t guid = 0;
		Script_GetGUIDFromToken(token, &guid, 0);
		if (guid)
		{
			uint32_t lvl = sUnitLevelCache.GetUnitItemLevelOrDungeonLevel(guid);
			if (lvl)
			{
				void* unit = ClntObjMgr::ObjectPtr(guid, TYPEMASK_UNIT);
				if (unit && !CGUnit_C::IsBossMob(unit))
				{
					FrameScript::PushNumber(L, static_cast<double>(lvl));
					return 1;
				}
			}
		}
	}
	return Script_UnitLevel(L);
}

int Script_TrueLevel(lua_State* L)
{
	return Script_UnitLevel(L);
}

CLIENT_DETOUR_THISCALL(CGNamePlateFrame__UpdateLevelDisplay, 0x0098EF10, void, (void* unit))
{
	CGUnit* cgUnit = static_cast<CGUnit*>(unit);
	CGUnit* cgPlayer = reinterpret_cast<CGUnit*>(ClntObjMgr::GetActivePlayerObj());
	if (!cgPlayer)
		return;

	uint64_t guid = cgUnit->objectBase.ObjectData->OBJECT_FIELD_GUID;
	uint32_t dlvl = sUnitLevelCache.GetUnitItemLevelOrDungeonLevel(guid);
	int displayLevel = dlvl ? static_cast<int>(dlvl)
	                        : (cgUnit->unitData ? static_cast<int>(cgUnit->unitData->level) : 0);

	uint8_t* selfBytes = reinterpret_cast<uint8_t*>(self);
	uint8_t* levelText = *reinterpret_cast<uint8_t**>(selfBytes + 0x2D4);
	uint8_t* skullIcon = *reinterpret_cast<uint8_t**>(selfBytes + 0x2B4);

	bool showLevel = !CGUnit_C::IsBossMob(cgUnit);

	if (showLevel)
	{
		levelText[0xCC] |= 0x10;
		CSimpleRegion::ShowThis(levelText);
		skullIcon[0xCC] &= 0xEF;
		CSimpleRegion::HideThis(skullIcon);

		uint64_t playerGuid = cgPlayer->objectBase.ObjectData->OBJECT_FIELD_GUID;
		uint32_t playerIlvl = sUnitLevelCache.GetPlayerItemLevel(playerGuid);
		int playerLevel = playerIlvl ? static_cast<int>(playerIlvl)
		                             : (cgPlayer->unitData ? static_cast<int>(cgPlayer->unitData->level) : 0);
		void* color = GetNameplateLevelColor(displayLevel, playerLevel);
		CSimpleRegion::SetVertexColor(levelText, color);

		char buf[32];
		sprintf_s(buf, "%d", displayLevel);
		CSimpleFontString::SetText(levelText, buf, 1);
	}
	else
	{
		levelText[0xCC] &= 0xEF;
		CSimpleRegion::HideThis(levelText);
		skullIcon[0xCC] |= 0x10;
		CSimpleRegion::ShowThis(skullIcon);
	}
}

void OnUnitPostInit(void* unit, double st0, int arg0, int arg4, int arg8)
{
	CGUnit* cgUnit = static_cast<CGUnit*>(unit);
	uint64_t guid = cgUnit->objectBase.ObjectData->OBJECT_FIELD_GUID;
	UnitLevelCache::SendRequest(guid);
	if (sUnitLevelCache.HasCreatureDungeonLevel(guid))
	{
		uint32_t dlvl = sUnitLevelCache.GetCreatureDungeonLevel(guid);
		sUnitLevelCache.ApplyCreatureDungeonLevel(cgUnit, dlvl);
	}
}

void OnUnitPostReenable(void* unit, double st0)
{
	CGUnit* cgUnit = static_cast<CGUnit*>(unit);
	uint64_t guid = cgUnit->objectBase.ObjectData->OBJECT_FIELD_GUID;
	sUnitLevelCache.SendRequest(guid);
	if (sUnitLevelCache.HasCreatureDungeonLevel(guid))
	{
		uint32_t dlvl = sUnitLevelCache.GetCreatureDungeonLevel(guid);
		sUnitLevelCache.ApplyCreatureDungeonLevel(cgUnit, dlvl);
	}
}

int OnUnitLevelChange(void* unit)
{
	CGUnit* cgUnit = static_cast<CGUnit*>(unit);
	uint64_t guid = cgUnit->objectBase.ObjectData->OBJECT_FIELD_GUID;

	// The local player keeps the original behaviour (level-up sparkle, etc.).
	if (guid == ClntObjMgr::GetActivePlayer())
		return 1;

	// uint32_t dlvl = sUnitLevelCache.GetUnitItemLevelOrDungeonLevel(guid);
	// if (dlvl && cgUnit->unitData)
	//     cgUnit->unitData->level = dlvl;

	CGUnit_C::UpdateModelScale(cgUnit, 0);

	void* namePlate = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(cgUnit) + 0xC38);
	if (namePlate)
		CGNamePlateFrame::UpdateLevelDisplay(namePlate, cgUnit);

	return 0;
}
