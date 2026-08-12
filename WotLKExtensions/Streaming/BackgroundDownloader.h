#pragma once

struct lua_State;

namespace Streaming
{
	class BackgroundDownloader
	{
	public:
		static BackgroundDownloader& Instance();

		BackgroundDownloader(const BackgroundDownloader&) = delete;
		BackgroundDownloader& operator=(const BackgroundDownloader&) = delete;

		void Start();

		void Trigger();

		void StartPolling();

		void PumpMainThread();

		bool IsActive();

		// Active download, or the first update check has not finished yet.
		bool IsBusy();

		static int Lua_IsStreaming(lua_State* L);
		static int Lua_IsBusy(lua_State* L);
		static int Lua_GetProgress(lua_State* L);
		static int Lua_RestartPending(lua_State* L);
		static int Lua_UIRefreshPending(lua_State* L);
		static int Lua_RefreshUI(lua_State* L);

	private:
		BackgroundDownloader() = default;
	};
}

#define sBackgroundDownloader Streaming::BackgroundDownloader::Instance()
