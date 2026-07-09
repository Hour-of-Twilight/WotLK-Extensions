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

		void PumpMainThread();

		bool IsActive();

		static int Lua_IsStreaming(lua_State* L);
		static int Lua_GetProgress(lua_State* L);
		static int Lua_RestartPending(lua_State* L);
		static int Lua_UIRefreshPending(lua_State* L);
		static int Lua_RefreshUI(lua_State* L);

	private:
		BackgroundDownloader() = default;
	};
}

#define sBackgroundDownloader Streaming::BackgroundDownloader::Instance()
