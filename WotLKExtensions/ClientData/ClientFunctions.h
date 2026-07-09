#pragma once
#include <Macros.h>
#include <ClientData/ObjectModel.h>
#include <ClientData/DBCRows.h>
#include <ClientData/GameStructs.h>
#include <ClientData/SharedDefines.h>

struct lua_State;

namespace CGActionBar
{
	CLIENT_FUNCTION(GetSpell, 0x005A8C30, __cdecl, uint32, (uint32 slot, uint32* outCategory))
	CLIENT_FUNCTION(UpdateUsableAction, 0x005A9E20, __cdecl, bool, (uint32 slot, bool* outNoMana))
	CLIENT_FUNCTION(UpdateUsable, 0x005AA470, __cdecl, int, ())
	CLIENT_FUNCTION(IsItem, 0x005A7890, __cdecl, bool, (uint32 slot))
	CLIENT_FUNCTION(IsSpell, 0x005A7860, __cdecl, bool, (uint32 slot))
	CLIENT_FUNCTION(PutActionInSlot, 0x005AB120, __cdecl, void, (int slot))
}

namespace CGItem_C
{
	CLIENT_FUNCTION(Lock, 0x00706FE0, __thiscall, void, (void* item))
	CLIENT_FUNCTION(Unlock, 0x00707020, __thiscall, void, (void* item))
	CLIENT_FUNCTION(GetItemByFullName, 0x00709DE0, __cdecl, uint32_t, (const char* name))
}

namespace CGBag_C
{
	CLIENT_FUNCTION(FindItemOfType, 0x00754A20, __thiscall, void*, (void* bagContainer, uint32_t entryId, uint32_t flags))
	CLIENT_FUNCTION(GetItemPointer, 0x00754390, __thiscall, void*, (void* bagContainer, int slot))
}

namespace CGNamePlateFrame
{
	CLIENT_FUNCTION(UpdateLevelDisplay, 0x0098EF10, __thiscall, void, (void* namePlate, void* unit))
}

namespace CGGameUI
{
	CLIENT_FUNCTION(GetCursorSpell, 0x005136C0, __cdecl, uint32_t, ())
	CLIENT_FUNCTION(GetCursorItem, 0x00513660, __cdecl, uint64_t, ())
	CLIENT_FUNCTION(ClearCursor, 0x00519280, __cdecl, void, (int arg0, int arg4))
}

namespace CGTooltip
{
	CLIENT_FUNCTION(SetSpell, 0x006238A0, __thiscall, int, (void* _this, uint32 spellId, int rank, int castTimeIdx, int isPetSpell, int nextRank1, int nextRank2, int talentRank1, int talentRank2, int unk1, int unk2, int unk3, int unk4, int showFlag, int cooldown, int owner))
	CLIENT_FUNCTION(AddLine, 0x0061FEC0, __thiscall, void, (void* _this, char* left, char* right, void* leftColor, void* rightColor, int wrap))
	CLIENT_FUNCTION(CalculateSize, 0x0061CAF0, __thiscall, void, (void* _this))
}

namespace CDataStore_C
{
	CLIENT_FUNCTION(GenPacket, 0x401050, __thiscall, void, (CDataStore*))
	CLIENT_FUNCTION(GetInt8, 0x47B340, __thiscall, void, (CDataStore*, int8_t*))
	CLIENT_FUNCTION(PutInt8, 0x47AFE0, __thiscall, void, (CDataStore*, int8_t))
	CLIENT_FUNCTION(GetInt16, 0x47B380, __thiscall, void, (CDataStore*, int16_t*))
	CLIENT_FUNCTION(GetInt32, 0x47B3C0, __thiscall, void, (CDataStore*, int32_t*))
	CLIENT_FUNCTION(GetUInt32, 0x47B3C0, __thiscall, void, (CDataStore*, uint32_t*))
	CLIENT_FUNCTION(PutInt32, 0x47B0A0, __thiscall, void, (CDataStore*, int32_t))
	CLIENT_FUNCTION(PutInt64, 0x47B100, __thiscall, void, (CDataStore*, int64_t))
	CLIENT_FUNCTION(PutFloat, 0x47B160, __thiscall, void, (CDataStore*, float))
	CLIENT_FUNCTION(GetInt64, 0x47B400, __thiscall, void, (CDataStore*, int64_t*))
	CLIENT_FUNCTION(GetFloat, 0x47B440, __thiscall, void, (CDataStore*, float*))
	CLIENT_FUNCTION(GetCString, 0x47B480, __thiscall, void, (CDataStore*, char*, uint32))
	CLIENT_FUNCTION(PutCString, 0x47B300, __thiscall, void, (CDataStore*, char* string))
	CLIENT_FUNCTION(Release, 0x403880, __thiscall, void, (CDataStore*))
	CLIENT_FUNCTION(FetchRead, 0x47B290, __thiscall, int, (CDataStore*, uint32_t offset, uint32_t size))
}

namespace CCharacterComponent
{
	CLIENT_FUNCTION(RemoveItemBySlot, 0x004EE6D0, __thiscall, void, (void* _this, int slot))
	CLIENT_FUNCTION(RemoveItem, 0x004EE6D0, __thiscall, void, (void* _this, int slot))
	CLIENT_FUNCTION(AddItem, 0x004F2640, __thiscall, void, (void* _this, int slot, void* itemDisplayInfoRecord, int unk))
	CLIENT_FUNCTION(AddItemBySlot, 0x004E10BA, __thiscall, void, (void* _this, int slot, int itemEntry, int unk))
	CLIENT_FUNCTION(AddItemByDisplayId, 0x004F2830, __thiscall, void, (void* _this, int slot, int displayId, int unk))
}

namespace CGPlayer_C
{
	CLIENT_FUNCTION(IsDeadOrGhost, 0x6DAC10, __thiscall, bool, (CGPlayer*))
	CLIENT_FUNCTION(SetComboPoints, 0x00519780, __cdecl, void, (uint64_t* targetGuid, uint8_t count))
	CLIENT_FUNCTION(RefreshVisibleItems, 0x006E09E0, __thiscall, int, (CGPlayer * _this))
	CLIENT_FUNCTION(UpdateXRayVision, 0x6D7490, __cdecl, int, (int arg))
	CLIENT_FUNCTION(EquipVisibleItem, 0x006E08C0, __thiscall, void, (CGPlayer * _this, int* itemEntry, int slot))
	CLIENT_FUNCTION(HasSpell, 0x0053C5B0, __cdecl, bool, (uint32 spellId))
	CLIENT_FUNCTION(UnitIsTrivial, 0x006DE130, __cdecl, bool, (void* unit))
}

namespace CSimpleRegion
{
	CLIENT_FUNCTION(HideThis, 0x00487BF0, __thiscall, void, (void* region))
	CLIENT_FUNCTION(ShowThis, 0x00487C40, __thiscall, void, (void* region))
	CLIENT_FUNCTION(SetVertexColor, 0x00487A10, __thiscall, void, (void* region, void* colorPtr))
}

namespace CSimpleFontString
{
	CLIENT_FUNCTION(SetText, 0x00483910, __thiscall, void, (void* fontStr, const char* text, int flag))
}

namespace CGUnit_C
{
	CLIENT_FUNCTION(InitializeComponent, 0x00730100, __thiscall, int, (CGUnit * pUnit))
	CLIENT_FUNCTION(GetDisplayClassNameFromRecord, 0x007159E0, __cdecl, const char*, (void* classRecord, int sex, int* outSex))
	CLIENT_FUNCTION(GetDisplayRaceNameFromRecord, 0x00715970, __cdecl, const char*, (void* raceRecord, int sex, int* outSex))
	CLIENT_FUNCTION(GetDisplayClassName, 0x0072AAB0, __cdecl, const char*, (void* unitObj, int displaySex))
	CLIENT_FUNCTION(IsBossMob, 0x00715D70, __thiscall, bool, (void* unit))
	CLIENT_FUNCTION(UnitReaction, 0x007251C0, __thiscall, int, (void* unit, void* player))
	CLIENT_FUNCTION(UpdateModelScale, 0x0072CBB0, __thiscall, void, (void* unit, int flags))
	CLIENT_FUNCTION(AnimationData, 0x007385C0, __thiscall, void, (CGUnit * unit, int animationId, char flags))
}

namespace ClientDB
{
	CLIENT_FUNCTION(GetGameTableValue, 0x7F6990, __cdecl, double, (uint32_t, uint32_t, uint32_t))
	CLIENT_FUNCTION(DecompressRow, 0x004CFBB0, __cdecl, void, (void* compressedData, int size, void* outBuffer))
}

namespace ClientServices
{
	CLIENT_FUNCTION(InitializePlayer, 0x6E83B0, __cdecl, void, ())
	CLIENT_FUNCTION(SendPacket, 0x6B0B50, __cdecl, void, (CDataStore*))
	CLIENT_FUNCTION(Connect, 0x6B1390, __thiscall, void, (void*))
	CLIENT_FUNCTION(GetCurrent, 0x4DA280, __cdecl, void*, ())
}

namespace ClntObjMgr
{
	CLIENT_FUNCTION(GetObjectNameFromGuid, 0x74D750, __cdecl, char*, (uint64_t))
	CLIENT_FUNCTION(EnumVisibleObjects, 0x004D4B30, __cdecl, int, (int(__cdecl * callback)(uint32_t guidLow, uint32_t guidHigh, void* userdata), void* userdata))
}

namespace CGGuildInfo_C
{
	CLIENT_FUNCTION(DbGuildCache_GetInfoBlockById, 0x0067D930, __thiscall, void*,
	    (void* cache, uint32_t guildId, uint32_t* playerGuidPair, void* callback, uint32_t callbackArg, uint8_t flags))
}

CLIENT_FUNCTION(CGGuildInfo__GuildNameCallback, 0x006D1C70, __cdecl, void, (void*))
CLIENT_FUNCTION(Script_GetGUIDFromToken, 0x0060ABF0, __cdecl, void, (const char* token, uint64_t* outGuid, char unused))
CLIENT_FUNCTION(Script_GetTokenFromGUID, 0x0060B102, __cdecl, const char*, (uint64_t guid))

namespace CNetClient
{
	CLIENT_FUNCTION(ProcessMessage, 0x631FE0, __thiscall, int, (void*, uint32_t, CDataStore*, uint32_t))
	CLIENT_FUNCTION(SetMessageHandler, 0x631FA0, __thiscall, void, (void*, uint32_t, void*, void*))
}

namespace CVar_C
{
	CLIENT_FUNCTION(SetCvar, 0x76C9C0, __thiscall, void, (void* cvar, const char* value, bool setDirty, bool setName2IfEmpty, bool setName1IfEmpty, bool forceUpdate))
	CLIENT_FUNCTION(LookupCvar, 0x767460, __cdecl, void*, (const char* name))

	inline CVar* GetCVar(const char* name)
	{
		return ((decltype(&GetCVar))0x00767460)(name);
	}
	inline CVar* FindCVar(const char* name)
	{
		return ((decltype(&FindCVar))0x00767440)(name);
	}
}

namespace FriendList_C
{
	CLIENT_FUNCTION(GetFriendInfoByName, 0x006B3570, __thiscall, uint8_t*, (void* self, const char* name))
}

namespace FrameScript
{
	CLIENT_FUNCTION(DisplayError, 0x84F280, __cdecl, void, (lua_State * L, const char*, ...))
	CLIENT_FUNCTION(GetNumber, 0x84E030, __cdecl, double, (lua_State*, int32_t))
	CLIENT_FUNCTION(ToLString, 0x84E0E0, __cdecl, char*, (lua_State*, int, bool))
	CLIENT_FUNCTION(IsNumber, 0x84DF20, __cdecl, int32_t, (lua_State*, int32_t))
	CLIENT_FUNCTION(LoadFunctions, 0x5120E0, __cdecl, int, ())
	CLIENT_FUNCTION(RegisterSimpleFrameScriptMethods, 0x0081B870, __cdecl, int, ())
	CLIENT_FUNCTION(PushBoolean, 0x84E4D0, __cdecl, int, (lua_State * L, int))
	CLIENT_FUNCTION(PushNil, 0x84E280, __cdecl, int, (lua_State*))
	CLIENT_FUNCTION(PushNumber, 0x84E2A0, __cdecl, int, (lua_State * L, double value))
	CLIENT_FUNCTION(PushString, 0x84E350, __cdecl, int, (lua_State*, char const*))
	CLIENT_FUNCTION(RegisterFunction, 0x817F90, __cdecl, int, (const char*, void*))
	CLIENT_FUNCTION(GetTop, 0x0084DBD0, __cdecl, int, (lua_State * L))
	CLIENT_FUNCTION(SetTop, 0x0084DBF0, __cdecl, void, (lua_State * L, int idx))
	CLIENT_FUNCTION(Insert, 0x0084DCC0, __cdecl, void, (lua_State * L, int idx))
	CLIENT_FUNCTION(PushValue, 0x0084DE50, __cdecl, void, (lua_State * L, int idx))
	CLIENT_FUNCTION(Type, 0x0084DEB0, __cdecl, int, (lua_State * L, int idx))
	CLIENT_FUNCTION(ToBoolean, 0x0084E0B0, __cdecl, bool, (lua_State * L, int idx))
	CLIENT_FUNCTION(ToUserdata, 0x0084E1C0, __cdecl, void*, (lua_State * L, int idx))
	CLIENT_FUNCTION(ObjLen, 0x0084E150, __cdecl, int, (lua_State * L, int idx))
	CLIENT_FUNCTION(PushCClosure, 0x0084E400, __cdecl, void, (lua_State * L, int(__cdecl* func)(lua_State*), int n))
	CLIENT_FUNCTION(GetField, 0x0084E590, __cdecl, void, (lua_State * L, int idx, const char* k))
	CLIENT_FUNCTION(RawGet, 0x0084E600, __cdecl, void, (lua_State * L, int idx))
	CLIENT_FUNCTION(RawGetI, 0x0084E670, __cdecl, void, (lua_State * L, int idx, int n))
	CLIENT_FUNCTION(CreateTable, 0x0084E6E0, __cdecl, void, (lua_State * L, int narr, int nrec))
	CLIENT_FUNCTION(SetField, 0x0084E900, __cdecl, void, (lua_State * L, int idx, const char* k))
	CLIENT_FUNCTION(RawSet, 0x0084E970, __cdecl, void, (lua_State * L, int idx))
	CLIENT_FUNCTION(RawSetI, 0x0084EA00, __cdecl, void, (lua_State * L, int idx, int n))
	CLIENT_FUNCTION(SetMetatable, 0x0084EA90, __cdecl, int, (lua_State * L, int idx))
	CLIENT_FUNCTION(PCall, 0x0084EC50, __cdecl, int, (lua_State * L, int nargs, int nresults, int errfunc))
	CLIENT_FUNCTION(Next, 0x0084EF50, __cdecl, int, (lua_State * L, int idx))
	CLIENT_FUNCTION(NewUserdata, 0x0084F0F0, __cdecl, void*, (lua_State * L, uint32_t size))
	CLIENT_FUNCTION(CheckType, 0x0084F960, __cdecl, void, (lua_State * L, int idx, int t))
	CLIENT_FUNCTION(CheckLString, 0x0084F9F0, __cdecl, const char*, (lua_State * L, int idx, uint32_t* len))
	CLIENT_FUNCTION(CheckNumber, 0x0084FAB0, __cdecl, double, (lua_State * L, int idx))
	CLIENT_FUNCTION(GetParamValue, 0x00815500, __cdecl, int, (lua_State * L, int idx, int default_))
}

namespace ItemDisplayInfoDB
{
	CLIENT_FUNCTION(GetRecord, 0x004CFD90, __thiscall, int, (void* _this, int itemDisplayInfoId, void* outRecord))
}

namespace Spell_C
{
	CLIENT_FUNCTION(GetCastTime, 0x7EF180, __cdecl, int, (SpellRow * pSpellRec, uint32_t hasInstantCast, int param3, int param4))
	CLIENT_FUNCTION(CancelSpell, 0x00806200, __cdecl, void, (void* spellCast, uint32_t arg1, uint32_t arg2, uint32_t cancelReason))
	CLIENT_FUNCTION(GetSpellModifiers, 0x007FD970, __cdecl, bool, (SpellRow * pSpellRec, uint32_t modifierType, int32_t* outFlatMod, int32_t* outPctMod))
}

namespace Tooltip
{
	CLIENT_FUNCTION(sub_61FEC0, 0x0061FEC0, __thiscall, void, (void* self, const char* text, char* unk1, int* unk2, int* unk3, int unk4))
}

namespace SStr
{
	CLIENT_FUNCTION(Copy_0, 0x76EF70, __stdcall, char*, (char*, char*, uint32_t))
	CLIENT_FUNCTION(SStrCopy, 0x0076E670, __cdecl, void, (char* dest, const char* src, int maxLen))
	CLIENT_FUNCTION(SStrCmp, 0x0076E760, __cdecl, int, (const char* a, const char* b, size_t maxLen))
}

namespace World
{
	CLIENT_FUNCTION(Pos3Dto2D, 0x4F6D20, __fastcall, int, (void* This, void* edx, C3Vector* pos3d, C3Vector* pos2d, uint32_t* flags))
}

namespace WowClientDb_C
{
	CLIENT_FUNCTION(GetRow, 0x008B7DA0, __thiscall, void, (void* row))
	CLIENT_FUNCTION(nullsub_3, 0x005EEB70, __thiscall, void, (void* row))
}

namespace MiscFunctions
{
	CLIENT_FUNCTION(GetPhysicalDamageClassID, 0x006337A0, __cdecl, int, ())
}

CLIENT_FUNCTION(sub_61FEC0, 0x61FEC0, __thiscall, void, (void*, char*, char*, void*, void*, uint32_t))
CLIENT_FUNCTION(CGAutoCompleteName__AddFlags, 0x0057B7C0, __cdecl, int, (void* guidPtr, int flags, const char* name, const char* extra))
CLIENT_FUNCTION(CGAutoCompleteName__Remove, 0x0057B000, __cdecl, void, (void* guidPtr, int flags))
CLIENT_FUNCTION(CGAutoCompleteName__RemoveFlagsFromAllNodes, 0x0057B0D0, __cdecl, int, (int flags))
CLIENT_FUNCTION(CCharacterSelection__UpdateCharacterList, 0x004E4610, __cdecl, int, ())
CLIENT_FUNCTION(ScrubString, 0x007E1830, __cdecl, char, (char* dest, char* src, int replaceChar, int maxLen, uint8_t flags))
CLIENT_FUNCTION(CCharacterCreation__CreateCharacter, 0x004E0380, __cdecl, void, (char* src))
CLIENT_FUNCTION(WowClientDB_GemProperties_GetRecord, 0x0065C290, __thiscall, void*, (void* _this, uint32_t id))
