#pragma once

#include <Defines.h>
#include <Macros.h>

#include <cstddef>

struct lua_State;

namespace ClientData::Lua
{
	using Alloc = void*(__cdecl*)(void* ud, void* ptr, size_t osize, size_t nsize);

	struct GlobalState
	{
		uint8 strt[0x0C];
		Alloc frealloc;
		void* ud;
		uint8 profile;
		uint8 currentwhite;
		uint8 gcstate;
		uint8 pad17;
		int32 sweepstrgc;
		void* rootgc;
		void** sweepgc;
		void* gray;
		void* grayagain;
		void* weak;
		void* tmudata;
		uint8 buff[0x0C];
		uint32 GCthreshold;
		uint32 totalbytes;
		uint32 estimate;
		uint32 gcdept;
		int32 gcpause;
		int32 gcstepmul;
	};

	static_assert(offsetof(GlobalState, frealloc) == 0x0C, "global_State::frealloc");
	static_assert(offsetof(GlobalState, gcstate) == 0x16, "global_State::gcstate");
	static_assert(offsetof(GlobalState, GCthreshold) == 0x40, "global_State::GCthreshold");
	static_assert(offsetof(GlobalState, totalbytes) == 0x44, "global_State::totalbytes");
	static_assert(offsetof(GlobalState, estimate) == 0x48, "global_State::estimate");
	static_assert(offsetof(GlobalState, gcdept) == 0x4C, "global_State::gcdept");
	static_assert(offsetof(GlobalState, gcpause) == 0x50, "global_State::gcpause");
	static_assert(offsetof(GlobalState, gcstepmul) == 0x54, "global_State::gcstepmul");

	struct State
	{
		void* next;
		const char* taint;
		uint8 tt;
		uint8 marked;
		uint8 status;
		uint8 pad0B;
		void* top;
		void* base;
		GlobalState* l_G;
		void* ci;
		void* savedpc;
		void* stack_last;
		void* stack;
		void* end_ci;
		void* base_ci;
		int32 stacksize;
		int32 size_ci;
		uint16 nCcalls;
		uint8 hookmask;
		uint8 allowhook;
		int32 basehookcount;
		int32 hookcount;
		void* hook;
	};

	static_assert(offsetof(State, l_G) == 0x14, "lua_State::l_G");
	static_assert(offsetof(State, ci) == 0x18, "lua_State::ci");
	static_assert(offsetof(State, base_ci) == 0x2C, "lua_State::base_ci");
	static_assert(offsetof(State, hookmask) == 0x3A, "lua_State::hookmask");
	static_assert(offsetof(State, basehookcount) == 0x3C, "lua_State::basehookcount");
	static_assert(offsetof(State, hookcount) == 0x40, "lua_State::hookcount");
	static_assert(offsetof(State, hook) == 0x44, "lua_State::hook");

	struct TValue
	{
		union
		{
			void* gc;
			void* p;
			double n;
			int32 b;
		} value;
		uint8 rest[8];
	};

	static_assert(sizeof(TValue) == 0x10, "TValue");

	struct CallInfo
	{
		TValue* base;
		TValue* func;
		TValue* top;
		void* savedpc;
		int32 nresults;
		int32 tailcalls;
	};

	static_assert(sizeof(CallInfo) == 0x18, "CallInfo");

	struct Closure
	{
		void* next;
		const char* taint;
		uint8 tt;
		uint8 marked;
		uint8 isC;
		uint8 nupvalues;
		void* gclist;
		void* env;
		void* profile;
		void* fOrProto;
	};

	static_assert(offsetof(Closure, fOrProto) == 0x18, "Closure::f / Closure::p");

	struct Debug
	{
		int32 event;
		const char* name;
		const char* namewhat;
		const char* what;
		const char* source;
		int32 currentline;
		int32 nups;
		int32 linedefined;
		int32 lastlinedefined;
		char short_src[60];
		int32 i_ci;
	};

	static_assert(sizeof(Debug) == 100, "lua_Debug");

	enum HookEvent : int32
	{
		HookCall = 0,
		HookRet = 1,
		HookLine = 2,
		HookCount = 3,
		HookTailRet = 4,
	};

	constexpr uint8 MaskCall = 1;
	constexpr uint8 MaskRet = 2;
	constexpr uint8 MaskLine = 4;
	constexpr uint8 MaskCount = 8;

	CLIENT_FUNCTION(GetInfo, 0x00850A90, __cdecl, int, (lua_State* L, char const* what, Debug* ar))

	enum GcState : uint8
	{
		GCSpause = 0,
		GCSpropagate = 1,
		GCSsweepstring = 2,
		GCSsweep = 3,
		GCSfinalize = 4,
	};

	enum GcOption : int
	{
		GCSTOP = 0,
		GCRESTART = 1,
		GCCOLLECT = 2,
		GCCOUNT = 3,
		GCCOUNTB = 4,
		GCSTEP = 5,
		GCSETPAUSE = 6,
		GCSETSTEPMUL = 7,
	};

	constexpr uint32 MaxThreshold = 0xFFFFFFFD;

	CLIENT_FUNCTION(GC, 0x0084ED50, __cdecl, int, (lua_State* L, int what, int data))
	CLIENT_FUNCTION(Step, 0x0085B950, __cdecl, void, (lua_State* L))

	inline State* Raw(lua_State* L)
	{
		return reinterpret_cast<State*>(L);
	}

	inline GlobalState* Global(lua_State* L)
	{
		return Raw(L)->l_G;
	}
}
