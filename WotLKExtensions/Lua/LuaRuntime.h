#pragma once

#include <Defines.h>

#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

struct lua_State;

namespace ClientData::Lua
{
	struct Debug;
}
class LuaPool;

class LuaRuntime
{
public:
	struct FrameStats
	{
		double handlerMs = 0;
		uint32 handlerCalls = 0;
		double gcMs = 0;
		uint32 gcSteps = 0;
		double allocKB = 0;
		double luaKB = 0;
		int gcState = 0;
		double frameMs = 0;
		double updateMs = 0;
		double layoutMs = 0;
		uint32 layoutFrames = 0;
		double buildMs = 0;
		double drawMs = 0;
		uint32 batches = 0;
		uint32 textures = 0;
		uint32 fontStrings = 0;
		uint32 texUploads = 0;
		double texUploadMs = 0;
		double buildStrata[9] = {};
		double drawStrata[9] = {};
		uint32 resizeFrom[8] = {};
		double raiseMs = 0;
		uint32 raises = 0;
		double compressMs = 0;
		uint32 compresses = 0;
		double occludeMs = 0;
		double addResizeMs = 0;
		uint32 addResizes = 0;
		uint32 layoutFailed = 0;
	};

	enum ResizeCaller : uint32
	{
		ResizePending = 0,
		ResizeForced,
		ResizeGetSize,
		ResizeShow,
		ResizeFlatten,
		ResizeUnflatten,
		ResizeDrag,
		ResizeOther,
		ResizeCallerCount,
	};

	static constexpr uint32 StrataCount = 9;
	static constexpr int GcPause = 110;
	static constexpr int GcStepMul = 200;
	static constexpr double GcBudgetMs = 1.0;
	static constexpr int GcSlackKB = 4096;

	static LuaRuntime& Instance();

	void Apply();

	void SetStatsEnabled(bool enabled);
	void SetHitchMs(float ms);
	void SetProfile(int mode);

	bool StatsEnabled() const
	{
		return m_statsEnabled;
	}
	FrameStats const& LastFrame() const
	{
		return m_last;
	}
	std::string const& PerfLogPath() const
	{
		return m_perfLogPath;
	}

	void* CreatePool();
	void AdoptPool(void* ud);
	bool DestroyPool(void* ud);

	void OnStateCreated();
	void BeginPaint();
	void OnHook(lua_State* L, ClientData::Lua::Debug* ar);
	static ResizeCaller ClassifyResizeCaller(uintptr_t returnAddress);
	void OnUiFrame();
	int IdleGcStep(lua_State* L, int what, int data);

	bool EnterHandler();
	void LeaveHandler(bool outer, int64 ticks);

	FrameStats& Cur()
	{
		return m_cur;
	}
	uint32 NextBuildSlot()
	{
		return m_buildIndex < StrataCount ? m_buildIndex++ : StrataCount - 1;
	}
	uint32 NextRenderSlot()
	{
		return m_renderIndex < StrataCount ? m_renderIndex++ : StrataCount - 1;
	}

	static int64 Now();
	static double ToMs(int64 ticks);

private:
	LuaRuntime() = default;
	LuaRuntime(const LuaRuntime&) = delete;
	LuaRuntime& operator=(const LuaRuntime&) = delete;

	void ApplyGcSettings(lua_State* L);
	void PaceGc(lua_State* L);
	void RollFrame(lua_State* L);
	void OpenPerfLog();
	void PerfLog(char const* line);
	void NoteSetting(char const* name, double value);
	static int FormatStrata(FrameStats const& stats, char* buffer, size_t size);
	static int FormatResizeFrom(FrameStats const& stats, char* buffer, size_t size);
	void InstallHook(lua_State* L);
	bool EnsureTrampoline();
	void WriteProfile();

	struct ProfileEntry
	{
		std::string name;
		uint32 calls = 0;
		int64 inclusive = 0;
		int64 self = 0;
	};

	struct ProfileFrame
	{
		void const* key;
		int64 start;
		int64 child;
		int32 ci;
	};

	static int Lua_GetLuaPerfStats(lua_State* L);
	static int Lua_GetUiPerfStats(lua_State* L);

	bool m_statsEnabled = false;
	float m_hitchMs = 0.0f;

	LuaPool* m_activePool = nullptr;
	uint64 m_lastAllocated = 0;
	FrameStats m_cur;
	FrameStats m_last;
	int m_handlerDepth = 0;
	int64 m_lastPaint = 0;
	uint32 m_buildIndex = 0;
	uint32 m_renderIndex = 0;
	std::ofstream m_perfLog;
	std::string m_perfLogPath;
	bool m_perfLogOpened = false;
	int m_profileMode = 0;
	void* m_trampoline = nullptr;
	bool m_trampolineFailed = false;
	lua_State* m_profileState = nullptr;
	std::unordered_map<void const*, ProfileEntry> m_profile;
	std::vector<ProfileFrame> m_profileStack;
};

#define sLuaRuntime LuaRuntime::Instance()
