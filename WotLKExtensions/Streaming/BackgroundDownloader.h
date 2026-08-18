#pragma once

#include <string>

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

	// True when the manifest says this file is on its way out: queued for download, downloading, or
	// already staged and waiting for the swap. Takes an absolute path.
	bool IsUpdatePending(const std::wstring& absPath);

	// Bumps whenever a pass changes what needs replacing or lands a new file. Callers that cache
	// per-file results compare it against the value they last scanned at.
	unsigned UpdateGeneration();

	// True for a download we parked next to the real file waiting for the swap, named
	// <stem>.new-<8 hex>.<ext>. Takes a bare file name, not a path.
	bool IsStagedName(const std::wstring& fileName);
}

#define sBackgroundDownloader Streaming::BackgroundDownloader::Instance()
