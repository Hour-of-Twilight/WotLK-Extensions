#include "LuaRuntime.h"
#include "LuaPool.h"
#include "CustomLua.h"

#include <ClientData/ClientFunctions.h>
#include <ClientData/LuaState.h>
#include <ClientDetours.h>
#include <Logger.h>
#include <Util.h>

#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <intrin.h>
#include <vector>

#pragma intrinsic(_ReturnAddress)

using namespace ClientData::Lua;

LuaRuntime& LuaRuntime::Instance()
{
	static LuaRuntime instance;
	return instance;
}

int64 LuaRuntime::Now()
{
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	return counter.QuadPart;
}

double LuaRuntime::ToMs(int64 ticks)
{
	static const double ticksPerMs = []
	{
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		return frequency.QuadPart / 1000.0;
	}();
	return ticks / ticksPerMs;
}

static int __cdecl IdleGcStepHook(lua_State* L, int what, int data)
{
	return sLuaRuntime.IdleGcStep(L, what, data);
}

static void FormatClock(char* buffer, size_t size)
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	std::snprintf(buffer, size, "%02u:%02u:%02u.%03u", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

namespace
{
	constexpr uintptr_t TrampolineAddress = 0x008481E1;
	constexpr size_t TrampolinePadding = 15;
	constexpr size_t ProfileTopN = 16;
}

static void __cdecl LuaProfileHook(lua_State* L, Debug* ar)
{
	sLuaRuntime.OnHook(L, ar);
}

void LuaRuntime::Apply()
{
	Util::OverwriteUInt32AtAddress(0x0052AFF3, (uint32)&IdleGcStepHook - 0x0052AFF7);
	sLua.RegisterFunction("GetLuaPerfStats", &Lua_GetLuaPerfStats, LuaFunctionState::ALL);
	sLua.RegisterFunction("GetUiPerfStats", &Lua_GetUiPerfStats, LuaFunctionState::ALL);

	char module[MAX_PATH];
	GetModuleFileNameA(nullptr, module, MAX_PATH);
	std::time_t now = std::time(nullptr);
	std::tm local = *std::localtime(&now);
	char name[64];
	std::strftime(name, sizeof(name), "uiperf_%Y-%m-%d_%H-%M-%S.log", &local);
	m_perfLogPath = (std::filesystem::path(module).parent_path() / "Logs" / name).string();
}

void LuaRuntime::SetStatsEnabled(bool enabled)
{
	m_statsEnabled = enabled;
	NoteSetting("luaPerfStats", enabled ? 1 : 0);
}

void LuaRuntime::SetHitchMs(float ms)
{
	m_hitchMs = ms;
	NoteSetting("luaPerfHitchMs", ms);
}

void LuaRuntime::SetProfile(int mode)
{
	m_profileMode = mode;
	if (lua_State* L = FrameScript::GetContext())
		InstallHook(L);
	NoteSetting("luaProfile", mode);
}

bool LuaRuntime::EnsureTrampoline()
{
	if (m_trampoline)
		return true;
	if (m_trampolineFailed)
		return false;

	uint8* at = reinterpret_cast<uint8*>(TrampolineAddress);
	for (size_t i = 0; i < TrampolinePadding; ++i)
	{
		if (at[i] != 0xCC)
		{
			m_trampolineFailed = true;
			sLog.Write("ERROR", "LuaRuntime", "luaProfile: the padding at 0x8481E1 is not free, the Lua profiler stays off");
			return false;
		}
	}

	DWORD oldProtect;
	if (!VirtualProtect(at, TrampolinePadding, PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		m_trampolineFailed = true;
		return false;
	}

	uint8 code[5] = { 0xE9, 0, 0, 0, 0 };
	uint32 rel = (uint32)(reinterpret_cast<uintptr_t>(&LuaProfileHook) - (TrampolineAddress + 5));
	std::memcpy(code + 1, &rel, sizeof(rel));
	std::memcpy(at, code, sizeof(code));
	VirtualProtect(at, TrampolinePadding, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), at, TrampolinePadding);
	m_trampoline = at;
	return true;
}

void LuaRuntime::InstallHook(lua_State* L)
{
	State* state = Raw(L);
	m_profileStack.clear();
	m_profile.clear();
	if (m_profileMode != 0 && EnsureTrampoline())
	{
		state->hook = m_trampoline;
		state->basehookcount = 0;
		state->hookcount = 0;
		state->hookmask = MaskCall | MaskRet;
		m_profileState = L;
		return;
	}
	state->hookmask = 0;
	state->hook = nullptr;
	m_profileState = nullptr;
}

void LuaRuntime::OnHook(lua_State* L, Debug* ar)
{
	if (L != m_profileState)
		return;

	int64 now = Now();
	if (ar->event == HookCall)
	{
		int32 ci = ar->i_ci;
		while (!m_profileStack.empty() && m_profileStack.back().ci >= ci)
			m_profileStack.pop_back();

		CallInfo* info = static_cast<CallInfo*>(Raw(L)->base_ci) + ci;
		Closure* closure = static_cast<Closure*>(info->func->value.gc);
		void const* key = closure->fOrProto;
		if (m_profile.find(key) == m_profile.end())
		{
			ProfileEntry& entry = m_profile[key];
			char name[192];
			if (closure->isC)
			{
				GetInfo(L, "n", ar);
				std::snprintf(name, sizeof(name), "%s  (C)", ar->name ? ar->name : "?");
			}
			else
			{
				GetInfo(L, "Sn", ar);
				char const* source = ar->source ? ar->source : ar->short_src;
				if (*source == '@' || *source == '=')
					++source;
				std::snprintf(name, sizeof(name), "%s:%d  %s", source, ar->linedefined, ar->name ? ar->name : "");
			}
			entry.name = name;
		}
		m_profileStack.push_back({ key, now, 0, ci });
		return;
	}

	if (ar->event != HookRet && ar->event != HookTailRet)
		return;
	if (m_profileStack.empty())
		return;

	if (ar->event == HookRet)
	{
		int32 ci = ar->i_ci;
		while (!m_profileStack.empty() && m_profileStack.back().ci > ci)
			m_profileStack.pop_back();
		if (m_profileStack.empty() || m_profileStack.back().ci != ci)
			return;
	}

	ProfileFrame frame = m_profileStack.back();
	m_profileStack.pop_back();
	int64 elapsed = now - frame.start;
	auto it = m_profile.find(frame.key);
	if (it != m_profile.end())
	{
		++it->second.calls;
		it->second.inclusive += elapsed;
		it->second.self += elapsed - frame.child;
	}
	if (!m_profileStack.empty())
		m_profileStack.back().child += elapsed;
}

void LuaRuntime::WriteProfile()
{
	if (m_profile.empty())
		return;

	std::vector<ProfileEntry const*> top;
	top.reserve(m_profile.size());
	int64 totalSelf = 0;
	for (auto const& kv : m_profile)
	{
		top.push_back(&kv.second);
		totalSelf += kv.second.self;
	}
	std::sort(top.begin(), top.end(), [](ProfileEntry const* a, ProfileEntry const* b)
	{
		return a->self > b->self;
	});

	char line[448];
	std::snprintf(line, sizeof(line), "#  profile: %u functions, %.1f ms of self time, top entries as self ms / inclusive ms / calls", (uint32)top.size(), ToMs(totalSelf));
	PerfLog(line);
	for (size_t i = 0; i < top.size() && i < ProfileTopN; ++i)
	{
		ProfileEntry const* entry = top[i];
		std::snprintf(line, sizeof(line), "#   %8.2f %8.2f %6u  %s", ToMs(entry->self), ToMs(entry->inclusive), entry->calls, entry->name.c_str());
		PerfLog(line);
	}
}

LuaRuntime::ResizeCaller LuaRuntime::ClassifyResizeCaller(uintptr_t returnAddress)
{
	if (returnAddress >= 0x004898B0 && returnAddress < 0x00489980)
		return ResizePending;
	if (returnAddress >= 0x00489DE0 && returnAddress < 0x00489E90)
		return ResizeForced;
	if (returnAddress >= 0x00489FF0 && returnAddress < 0x0048A130)
		return ResizeGetSize;
	if (returnAddress >= 0x0048A520 && returnAddress < 0x0048A5F0)
		return ResizeShow;
	if (returnAddress >= 0x0048A5F0 && returnAddress < 0x0048A840)
		return ResizeFlatten;
	if (returnAddress >= 0x0048A840 && returnAddress < 0x0048ABD0)
		return ResizeUnflatten;
	if (returnAddress >= 0x0048ABD0 && returnAddress < 0x0048B130)
		return ResizeDrag;
	return ResizeOther;
}

int LuaRuntime::FormatResizeFrom(FrameStats const& stats, char* buffer, size_t size)
{
	return std::snprintf(buffer, size, "p%u f%u g%u s%u fl%u uf%u d%u o%u",
	    stats.resizeFrom[ResizePending], stats.resizeFrom[ResizeForced], stats.resizeFrom[ResizeGetSize], stats.resizeFrom[ResizeShow],
	    stats.resizeFrom[ResizeFlatten], stats.resizeFrom[ResizeUnflatten], stats.resizeFrom[ResizeDrag], stats.resizeFrom[ResizeOther]);
}

void LuaRuntime::OpenPerfLog()
{
	if (m_perfLogOpened)
		return;
	m_perfLogOpened = true;
	if (m_perfLogPath.empty())
		return;

	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(m_perfLogPath).parent_path(), ec);
	m_perfLog.open(m_perfLogPath, std::ios::out | std::ios::trunc);
	if (!m_perfLog.is_open())
		return;

	char clock[32];
	FormatClock(clock, sizeof(clock));
	char line[512];
	std::snprintf(line, sizeof(line), "# HoTDLL UI perf log, one file per client start, opened at %s on the first hitch. One row per frame whose Lua plus GC time or update plus build plus draw time reached luaPerfHitchMs. Times are ms.", clock);
	m_perfLog << line << "\n";
	std::snprintf(line, sizeof(line), "# settings: luaPerfHitchMs %.1f, luaProfile %d", m_hitchMs, m_profileMode);
	m_perfLog << line << "\n";
	m_perfLog << "time\tframe\tlua\tcalls\tgc\tsteps\tupdate\tlayout\tlayoutFrames\tresizeFrom\tbuild\ttextures\tstrings\tdraw\tworld\tbatches\ttexUploads\ttexUploadMs\tallocKB\theapKB\traise/compress/occlude\tresizeList\tstrata build/draw\n";
	m_perfLog.flush();
	sLog.Write("INFO", "LuaRuntime", ("UI perf log: " + m_perfLogPath).c_str());
}

void LuaRuntime::PerfLog(char const* line)
{
	OpenPerfLog();
	if (!m_perfLog.is_open())
		return;
	m_perfLog << line << "\n";
	m_perfLog.flush();
}

void LuaRuntime::NoteSetting(char const* name, double value)
{
	if (!m_perfLog.is_open())
		return;
	char clock[32];
	FormatClock(clock, sizeof(clock));
	char line[128];
	std::snprintf(line, sizeof(line), "# %s %s = %g", clock, name, value);
	PerfLog(line);
}

int LuaRuntime::FormatStrata(FrameStats const& stats, char* buffer, size_t size)
{
	static char const* const names[StrataCount] = { "W", "BG", "LOW", "MED", "HIGH", "DLG", "FS", "FSD", "TIP" };
	int len = 0;
	buffer[0] = 0;
	for (uint32 i = 0; i < StrataCount && len < (int)size; ++i)
	{
		int written = std::snprintf(buffer + len, size - len, "%s%s %.1f/%.1f", i ? "  " : "", names[i], stats.buildStrata[i], stats.drawStrata[i]);
		if (written < 0)
			break;
		len += written;
	}
	return len;
}

void LuaRuntime::BeginPaint()
{
	int64 now = Now();
	if (m_lastPaint)
		m_cur.frameMs = ToMs(now - m_lastPaint);
	m_lastPaint = now;
	m_buildIndex = 0;
	m_renderIndex = 0;
}

void LuaRuntime::ApplyGcSettings(lua_State* L)
{
	GC(L, GCSETPAUSE, GcPause);
	GC(L, GCSETSTEPMUL, GcStepMul);
}

void* LuaRuntime::CreatePool()
{
	return new LuaPool();
}

void LuaRuntime::AdoptPool(void* ud)
{
	m_activePool = static_cast<LuaPool*>(ud);
	m_lastAllocated = 0;
}

bool LuaRuntime::DestroyPool(void* ud)
{
	if (!LuaPool::Is(ud))
		return false;

	if (m_activePool == ud)
	{
		m_activePool = nullptr;
		m_lastAllocated = 0;
	}
	delete static_cast<LuaPool*>(ud);
	return true;
}

void LuaRuntime::OnStateCreated()
{
	if (lua_State* L = FrameScript::GetContext())
	{
		ApplyGcSettings(L);
		InstallHook(L);
	}
	m_cur = FrameStats();
	m_handlerDepth = 0;
}

int LuaRuntime::IdleGcStep(lua_State* L, int what, int data)
{
	return 0;
}

void LuaRuntime::OnUiFrame()
{
	lua_State* L = FrameScript::GetContext();
	if (!L)
		return;

	PaceGc(L);
	if (m_statsEnabled)
		RollFrame(L);
}

void LuaRuntime::PaceGc(lua_State* L)
{
	State* state = Raw(L);
	GlobalState* g = state->l_G;
	if (state->ci != state->base_ci)
		return;

	int64 start = Now();
	uint64 pauseThreshold = (uint64)g->estimate * (uint64)GcPause / 100;
	bool idle = g->gcstate == GCSpause && (uint64)g->totalbytes < pauseThreshold;
	if (!idle)
	{
		double budget = GcBudgetMs;
		if (pauseThreshold && g->totalbytes > pauseThreshold)
			budget *= std::min(4.0, (double)g->totalbytes / (double)pauseThreshold);

		do
		{
			g->GCthreshold = g->totalbytes;
			Step(L);
			++m_cur.gcSteps;
			if (g->gcstate == GCSpause)
				break;
		} while (ToMs(Now() - start) < budget);
	}

	uint64 next = (uint64)g->totalbytes + (uint64)GcSlackKB * 1024;
	if (g->gcstate == GCSpause && pauseThreshold > next)
		next = pauseThreshold;
	g->GCthreshold = (uint32)std::min<uint64>(next, MaxThreshold);
	m_cur.gcMs += ToMs(Now() - start);
}

void LuaRuntime::RollFrame(lua_State* L)
{
	if (m_activePool)
	{
		uint64 total = m_activePool->BytesAllocated();
		m_cur.allocKB = (double)(total - m_lastAllocated) / 1024.0;
		m_lastAllocated = total;
	}
	else
		m_cur.allocKB = -1;

	m_cur.luaKB = GC(L, GCCOUNT, 0) + GC(L, GCCOUNTB, 0) / 1024.0;
	m_cur.gcState = Global(L)->gcstate;
	m_last = m_cur;
	m_cur = FrameStats();

	double paintMs = m_last.updateMs + m_last.buildMs + m_last.drawMs;
	if (m_hitchMs > 0 && (m_last.handlerMs + m_last.gcMs >= m_hitchMs || paintMs >= m_hitchMs))
	{
		char clock[32];
		FormatClock(clock, sizeof(clock));
		char strata[256];
		FormatStrata(m_last, strata, sizeof(strata));
		char resizeFrom[96];
		FormatResizeFrom(m_last, resizeFrom, sizeof(resizeFrom));
		char raise[96];
		std::snprintf(raise, sizeof(raise), "r%.2f/%u c%.2f/%u o%.2f", m_last.raiseMs, m_last.raises, m_last.compressMs, m_last.compresses, m_last.occludeMs);
		char resizeList[64];
		std::snprintf(resizeList, sizeof(resizeList), "a%.2f/%u fail%u", m_last.addResizeMs, m_last.addResizes, m_last.layoutFailed);
		char line[1024];
		std::snprintf(line, sizeof(line), "%s\t%.1f\t%.2f\t%u\t%.2f\t%u\t%.2f\t%.2f\t%u\t%s\t%.2f\t%u\t%u\t%.2f\t%.2f\t%u\t%u\t%.2f\t%.0f\t%.0f\t%s\t%s\t%s",
		    clock, m_last.frameMs, m_last.handlerMs, m_last.handlerCalls, m_last.gcMs, m_last.gcSteps, m_last.updateMs, m_last.layoutMs, m_last.layoutFrames, resizeFrom,
		    m_last.buildMs, m_last.textures, m_last.fontStrings, m_last.drawMs, m_last.drawStrata[0], m_last.batches, m_last.texUploads, m_last.texUploadMs,
		    m_last.allocKB, m_last.luaKB, raise, resizeList, strata);
		PerfLog(line);
		if (m_profileMode != 0)
			WriteProfile();
	}
	m_profile.clear();
	m_profileStack.clear();
}

bool LuaRuntime::EnterHandler()
{
	return m_handlerDepth++ == 0;
}

void LuaRuntime::LeaveHandler(bool outer, int64 ticks)
{
	--m_handlerDepth;
	++m_cur.handlerCalls;
	if (outer)
		m_cur.handlerMs += ToMs(ticks);
}

int LuaRuntime::Lua_GetLuaPerfStats(lua_State* L)
{
	LuaRuntime& runtime = Instance();
	FrameStats const& stats = runtime.m_last;
	FrameScript::PushNumber(L, stats.handlerMs);
	FrameScript::PushNumber(L, stats.handlerCalls);
	FrameScript::PushNumber(L, stats.gcMs);
	FrameScript::PushNumber(L, stats.gcSteps);
	FrameScript::PushNumber(L, stats.allocKB);
	FrameScript::PushNumber(L, stats.luaKB);
	FrameScript::PushNumber(L, stats.gcState);
	FrameScript::PushNumber(L, 1);
	FrameScript::PushBoolean(L, runtime.m_activePool != nullptr ? 1 : 0);
	return 9;
}

int LuaRuntime::Lua_GetUiPerfStats(lua_State* L)
{
	FrameStats const& stats = Instance().m_last;
	FrameScript::PushNumber(L, stats.frameMs);
	FrameScript::PushNumber(L, stats.updateMs);
	FrameScript::PushNumber(L, stats.layoutMs);
	FrameScript::PushNumber(L, stats.layoutFrames);
	FrameScript::PushNumber(L, stats.buildMs);
	FrameScript::PushNumber(L, stats.drawMs);
	FrameScript::PushNumber(L, stats.drawStrata[0]);
	FrameScript::PushNumber(L, stats.batches);
	FrameScript::PushNumber(L, stats.textures);
	FrameScript::PushNumber(L, stats.fontStrings);
	FrameScript::PushNumber(L, stats.texUploads);
	FrameScript::PushNumber(L, stats.texUploadMs);

	char strata[256];
	FormatStrata(stats, strata, sizeof(strata));
	FrameScript::PushString(L, strata);
	FrameScript::PushString(L, Instance().m_perfLogPath.c_str());
	char resizeFrom[96];
	FormatResizeFrom(stats, resizeFrom, sizeof(resizeFrom));
	FrameScript::PushString(L, resizeFrom);
	return 15;
}

CLIENT_DETOUR_THISCALL(UiLayerUpdate, 0x00495320, void*, (float elapsed))
{
	if (!sLuaRuntime.StatsEnabled())
		return UiLayerUpdate(self, elapsed);

	int64 start = LuaRuntime::Now();
	void* result = UiLayerUpdate(self, elapsed);
	sLuaRuntime.Cur().updateMs += LuaRuntime::ToMs(LuaRuntime::Now() - start);
	return result;
}

CLIENT_DETOUR(UiResizePending, 0x004898B0, __cdecl, int, ())
{
	if (!sLuaRuntime.StatsEnabled())
		return UiResizePending();

	int64 start = LuaRuntime::Now();
	int result = UiResizePending();
	sLuaRuntime.Cur().layoutMs += LuaRuntime::ToMs(LuaRuntime::Now() - start);
	return result;
}

CLIENT_DETOUR_THISCALL_NOARGS(UiFrameResize, 0x00489570, int)
{
	if (!sLuaRuntime.StatsEnabled())
		return UiFrameResize(self);

	LuaRuntime::FrameStats& cur = sLuaRuntime.Cur();
	++cur.layoutFrames;
	++cur.resizeFrom[LuaRuntime::ClassifyResizeCaller(reinterpret_cast<uintptr_t>(_ReturnAddress()))];
	int result = UiFrameResize(self);
	if (!result)
		++cur.layoutFailed;
	return result;
}

CLIENT_DETOUR_THISCALL(UiBuildBatches, 0x00494EE0, int, (int flag))
{
	if (!sLuaRuntime.StatsEnabled())
		return UiBuildBatches(self, flag);

	int64 start = LuaRuntime::Now();
	int result = UiBuildBatches(self, flag);
	double ms = LuaRuntime::ToMs(LuaRuntime::Now() - start);
	LuaRuntime::FrameStats& cur = sLuaRuntime.Cur();
	cur.buildMs += ms;
	cur.buildStrata[sLuaRuntime.NextBuildSlot()] += ms;
	return result;
}

CLIENT_DETOUR_THISCALL_NOARGS(UiRenderBatch, 0x00494F30, void)
{
	if (!sLuaRuntime.StatsEnabled())
	{
		UiRenderBatch(self);
		return;
	}

	int64 start = LuaRuntime::Now();
	UiRenderBatch(self);
	double ms = LuaRuntime::ToMs(LuaRuntime::Now() - start);
	LuaRuntime::FrameStats& cur = sLuaRuntime.Cur();
	cur.drawMs += ms;
	cur.drawStrata[sLuaRuntime.NextRenderSlot()] += ms;
}

CLIENT_DETOUR(UiDrawBatch, 0x00484B00, __cdecl, void, (int batch))
{
	if (sLuaRuntime.StatsEnabled())
		++sLuaRuntime.Cur().batches;
	UiDrawBatch(batch);
}

CLIENT_DETOUR_THISCALL(UiQueueTexture, 0x004847A0, void, (int region))
{
	if (sLuaRuntime.StatsEnabled())
		++sLuaRuntime.Cur().textures;
	UiQueueTexture(self, region);
}

CLIENT_DETOUR_THISCALL(UiQueueFontString, 0x004843A0, void, (int fontString))
{
	if (sLuaRuntime.StatsEnabled())
		++sLuaRuntime.Cur().fontStrings;
	UiQueueFontString(self, fontString);
}

CLIENT_DETOUR_THISCALL(GxTexUpload, 0x006A2D80, void, (void* texture))
{
	if (!sLuaRuntime.StatsEnabled())
	{
		GxTexUpload(self, texture);
		return;
	}

	int64 start = LuaRuntime::Now();
	GxTexUpload(self, texture);
	LuaRuntime::FrameStats& cur = sLuaRuntime.Cur();
	cur.texUploadMs += LuaRuntime::ToMs(LuaRuntime::Now() - start);
	++cur.texUploads;
}

CLIENT_DETOUR(luaM_initPool, 0x00855740, __cdecl, void*, ())
{
	return sLuaRuntime.CreatePool();
}

CLIENT_DETOUR(lua_newstate, 0x00855370, __cdecl, lua_State*, (ClientData::Lua::Alloc f, void* ud, int profile))
{
	if (!LuaPool::Is(ud))
		return lua_newstate(f, ud, profile);

	lua_State* L = lua_newstate(&LuaPool::Realloc, ud, profile);
	sLuaRuntime.AdoptPool(ud);
	return L;
}

CLIENT_DETOUR(luaM_freePool, 0x008557D0, __cdecl, void, (void* pools))
{
	if (!sLuaRuntime.DestroyPool(pools))
		luaM_freePool(pools);
}

CLIENT_DETOUR(FrameScript_Initialize, 0x00819BB0, __cdecl, void, (int profileEnabled))
{
	FrameScript_Initialize(profileEnabled);
	sLuaRuntime.OnStateCreated();
}

CLIENT_DETOUR(FrameScript_ExecuteHandler, 0x00819EA0, __cdecl, void, (int function, void* object, int args, const char* argsFmt, void* eventObject))
{
	if (!sLuaRuntime.StatsEnabled())
	{
		FrameScript_ExecuteHandler(function, object, args, argsFmt, eventObject);
		return;
	}

	bool outer = sLuaRuntime.EnterHandler();
	int64 start = LuaRuntime::Now();
	FrameScript_ExecuteHandler(function, object, args, argsFmt, eventObject);
	sLuaRuntime.LeaveHandler(outer, LuaRuntime::Now() - start);
}

CLIENT_DETOUR(PaintScreen, 0x00495810, __cdecl, int, (int a1, int a2, int a3, float a4))
{
	if (sLuaRuntime.StatsEnabled())
		sLuaRuntime.BeginPaint();
	int result = PaintScreen(a1, a2, a3, a4);
	sLuaRuntime.OnUiFrame();
	return result;
}
