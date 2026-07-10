#include "BackgroundDownloader.h"

#include "LauncherManifest.h"
#include "HttpClient.h"
#include "Sha256.h"

#include <ClientData/Streaming.h>

#include "DbcFromMpq.h"
#include <ClientData/SharedDefines.h>
#include <ClientData/Spell.h>
#include <ClientData/ObjectManager.h>
#include <ClientDetours.h>
#include <ClientData/ClientFunctions.h>
#include <Helpers/Util.h>
#include <Logger.h>
#include "Lua/CustomLua.h"
#include "Lua/XMLExtensions.h"

#include <windows.h>

#include <atomic>
#include <thread>
#include <mutex>
#include <string>
#include <vector>
#include <utility>
#include <filesystem>
#include <fstream>
#include <cctype>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <iterator>

namespace fs = std::filesystem;

namespace Streaming
{
	namespace
	{
		std::atomic<bool> g_started{ false };
		std::atomic<bool> g_running{ false };
		std::wstring g_installDir;

		std::atomic<bool> g_active{ false };
		std::atomic<long long> g_baseBytes{ 0 };
		std::atomic<long long> g_doneBytes{ 0 };
		std::atomic<long long> g_totalBytes{ 0 };
		std::atomic<int> g_filesDone{ 0 };
		std::atomic<int> g_filesTotal{ 0 };
		std::atomic<bool> g_restartNeeded{ false };
		std::atomic<bool> g_uiRefreshNeeded{ false };
		std::mutex g_statusMutex;
		std::string g_currentFile;

		void StreamLog(const char* fmt, ...)
		{
			char buf[1024];
			va_list a;
			va_start(a, fmt);
			std::vsnprintf(buf, sizeof(buf), fmt, a);
			va_end(a);
			sLog.Write("DEBUG", "stream", buf);
		}

		constexpr int kMountPriority = 1000000;

		std::wstring Widen(const std::string& s)
		{
			return std::wstring(s.begin(), s.end());
		}

		std::string NarrowAcp(const std::wstring& s)
		{
			if (s.empty())
				return "";
			int n = WideCharToMultiByte(CP_ACP, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
			std::string out(n, '\0');
			WideCharToMultiByte(CP_ACP, 0, s.c_str(), (int)s.size(), out.data(), n, nullptr, nullptr);
			return out;
		}

		std::wstring WidenAcp(const std::string& s)
		{
			if (s.empty())
				return L"";
			int n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), nullptr, 0);
			std::wstring out(n, L'\0');
			MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), out.data(), n);
			return out;
		}

		bool IsExtensionDll(const std::string& path)
		{
			size_t slash = path.find_last_of("/\\");
			std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
			for (char& c : name)
				c = (char)std::tolower((unsigned char)c);
			return name == "wotlkextensions.dll";
		}

		bool IsMpq(const std::string& path)
		{
			size_t dot = path.find_last_of('.');
			if (dot == std::string::npos)
				return false;
			std::string ext = path.substr(dot);
			for (char& c : ext)
				c = (char)std::tolower((unsigned char)c);
			return ext == ".mpq";
		}

		std::wstring StagedPath(const std::wstring& local, bool isMpq)
		{
			if (!isMpq)
				return local + L".new";
			size_t dot = local.find_last_of(L'.');
			if (dot == std::wstring::npos)
				return local + L".new.mpq";
			return local.substr(0, dot) + L".new" + local.substr(dot);
		}
		std::wstring AppendQuery(const std::wstring& url, const std::wstring& key, const std::wstring& value)
		{
			wchar_t sep = (url.find(L'?') == std::wstring::npos) ? L'?' : L'&';
			return url + sep + key + L"=" + value;
		}

		std::string ReadSmallFile(const std::wstring& p)
		{
			std::ifstream f(p, std::ios::binary);
			if (!f)
				return "";
			std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
			return s;
		}

		void WriteSmallFile(const std::wstring& p, const std::string& s)
		{
			std::ofstream f(p, std::ios::trunc | std::ios::binary);
			if (f)
				f.write(s.data(), (std::streamsize)s.size());
		}

		std::string UrlEncodePath(const std::string& p)
		{
			static const char* hex = "0123456789ABCDEF";
			std::string o;
			for (unsigned char c : p)
			{
				if (std::isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~')
					o += (char)c;
				else
				{
					o += '%';
					o += hex[c >> 4];
					o += hex[c & 0xF];
				}
			}
			return o;
		}

		long long FileSize(const std::wstring& p)
		{
			std::error_code ec;
			auto s = fs::file_size(p, ec);
			return ec ? -1 : (long long)s;
		}

		fs::path PendingFile()
		{
			return fs::path(g_installDir) / "Cache" / "hotstream-pending.txt";
		}

		void SpawnCloseSwapHelper()
		{
			static std::atomic<bool> spawned{ false };
			if (spawned.exchange(true))
				return;

			const DWORD pid = GetCurrentProcessId();
			const std::string pend = NarrowAcp(PendingFile().wstring());
			fs::path cmdPath = fs::path(g_installDir) / "Cache" / "hotstream-swap.cmd";

			{
				std::ofstream b(cmdPath, std::ios::trunc | std::ios::binary);
				if (!b)
					return;
				b << "@echo off\r\n";
				b << ":wait\r\n";
				b << "tasklist /fi \"PID eq " << pid << "\" /nh 2>nul | find \"" << pid
				  << "\" >nul && ( ping -n 2 127.0.0.1 >nul & goto wait )\r\n";
				b << "for /f \"usebackq tokens=1,2 delims=|\" %%a in (\"" << pend
				  << "\") do move /y \"%%a\" \"%%b\" >nul 2>&1\r\n";
				b << "del /q \"" << pend << "\" >nul 2>&1\r\n";
				b << "del /q \"%~f0\" >nul 2>&1\r\n";
			}

			std::wstring cmdline = L"cmd.exe /c \"" + cmdPath.wstring() + L"\"";
			std::vector<wchar_t> cl(cmdline.begin(), cmdline.end());
			cl.push_back(0);
			STARTUPINFOW si{};
			si.cb = sizeof(si);
			si.dwFlags = STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_HIDE;
			PROCESS_INFORMATION pi{};
			if (CreateProcessW(nullptr, cl.data(), nullptr, nullptr, FALSE,
			        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
			{
				CloseHandle(pi.hThread);
				CloseHandle(pi.hProcess);
				StreamLog("stream: spawned close-swap helper");
			}
			else
			{
				StreamLog("stream: close-swap helper spawn FAILED (err %lu)", GetLastError());
			}
		}

		std::wstring ToBackslashes(std::wstring p)
		{
			for (wchar_t& c : p)
				if (c == L'/')
					c = L'\\';
			return p;
		}

		void RecordPending(const std::wstring& newPath, const std::wstring& target, bool needsRestart)
		{
			std::string entry = NarrowAcp(ToBackslashes(newPath)) + "|" + NarrowAcp(ToBackslashes(target));

			std::ifstream in(PendingFile(), std::ios::binary);
			std::string line;
			while (in && std::getline(in, line))
			{
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				if (line == entry)
				{
					if (needsRestart)
						g_restartNeeded = true;
					SpawnCloseSwapHelper();
					return;
				}
			}
			in.close();

			std::error_code ec;
			fs::create_directories(PendingFile().parent_path(), ec);
			std::ofstream f(PendingFile(), std::ios::app | std::ios::binary);
			if (f)
				f << entry << "\r\n";
			if (needsRestart)
				g_restartNeeded = true;
			SpawnCloseSwapHelper();
		}

		void ApplyPending()
		{
			std::ifstream in(PendingFile(), std::ios::binary);
			if (!in)
				return;
			std::vector<std::string> keep;
			std::string line;
			while (std::getline(in, line))
			{
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				size_t bar = line.find('|');
				if (bar == std::string::npos)
					continue;
				std::string np = line.substr(0, bar), tg = line.substr(bar + 1);
				if (np.empty() || tg.empty())
					continue;
				std::wstring wnp = WidenAcp(np), wtg = WidenAcp(tg);
				std::error_code ec;
				if (!fs::exists(wnp, ec))
					continue;
				if (!MoveFileExW(wnp.c_str(), wtg.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
					keep.push_back(line);
			}
			in.close();
			std::ofstream out(PendingFile(), std::ios::trunc | std::ios::binary);
			for (const std::string& l : keep)
				out << l << "\r\n";
		}

		std::mutex g_mountMutex;
		std::vector<std::string> g_mountQueue;

		void EnqueueMount(const std::wstring& path)
		{
			std::string p = NarrowAcp(path);
			StreamLog("stream: queued mount %s", p.c_str());
			std::lock_guard<std::mutex> lock(g_mountMutex);
			g_mountQueue.push_back(std::move(p));
		}

		void PlaceFile(const std::string& baseUrl, const ManifestFile& mf, long long bps)
		{
			std::wstring local = (fs::path(g_installDir) / fs::path(mf.path)).wstring();
			std::error_code ec;
			fs::create_directories(fs::path(local).parent_path(), ec);
			std::wstring part = local + L".part";
			std::wstring meta = part + L".meta";

			// A .part left over from a previous run can be resumed with a Range request, but only
			// if the sidecar confirms those bytes belong to this same manifest entry (size + hash).
			std::string expected = std::to_string(mf.size) + ":" + mf.sha256;
			long long resume = 0;
			long long ps = FileSize(part);
			if (ps > 0 && ps < mf.size && ReadSmallFile(meta) == expected)
				resume = ps;
			else if (ps >= 0)
				fs::remove(part, ec);
			WriteSmallFile(meta, expected);

			std::string url = baseUrl;
			if (!url.empty() && url.back() == '/')
				url.pop_back();
			url += "/" + UrlEncodePath(mf.path) + "?v=" + mf.sha256;

			DownloadOptions opt;
			opt.resumeFrom = resume;
			opt.bytesPerSecond = bps;
			opt.onBytes = [](long long written)
			{
				g_doneBytes = g_baseBytes.load() + written;
			};
			if (!HttpDownloadFile(Widen(url), part, opt))
			{
				StreamLog("stream: download failed %s", mf.path.c_str());
				return;
			}

			std::string got = Sha256File(part);
			if (_stricmp(got.c_str(), mf.sha256.c_str()) != 0)
			{
				fs::remove(part, ec);
				fs::remove(meta, ec);
				StreamLog("stream: hash mismatch %s", mf.path.c_str());
				return;
			}
			fs::remove(meta, ec);

			bool isMpq = IsMpq(mf.path);
			bool existed = fs::exists(local, ec);

			if (!existed)
			{
				fs::rename(part, local, ec);
				if (ec)
				{
					fs::copy_file(part, local, fs::copy_options::overwrite_existing, ec);
					fs::remove(part, ec);
				}
				if (isMpq)
					EnqueueMount(local);
				return;
			}

			// Exists: swap now if unlocked, else stage .new, mount it, and swap later.
			if (MoveFileExW(part.c_str(), local.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				if (isMpq)
					EnqueueMount(local);
			}
			else
			{
				std::wstring np = StagedPath(local, isMpq);
				if (!MoveFileExW(part.c_str(), np.c_str(), MOVEFILE_REPLACE_EXISTING))
				{
					fs::remove(part, ec);
					StreamLog("stream: could not stage %s", mf.path.c_str());
					return;
				}
				if (isMpq)
					EnqueueMount(np);
				RecordPending(np, local, !isMpq);
				StreamLog("stream: staged %s (in use), will swap on next launch", mf.path.c_str());
			}
		}

		void SwapOrStage(const std::wstring& np, const std::wstring& local, bool isMpq)
		{
			if (MoveFileExW(np.c_str(), local.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				if (isMpq)
					EnqueueMount(local);
			}
			else
			{
				if (isMpq)
					EnqueueMount(np);
				RecordPending(np, local, !isMpq);
			}
		}

		struct ActiveScope
		{
			~ActiveScope()
			{
				g_active = false;
				std::lock_guard<std::mutex> lock(g_statusMutex);
				g_currentFile.clear();
			}
		};

		HANDLE g_patchLock = nullptr;

		bool AcquirePatchLock()
		{
			uint64_t h = 1469598103934665603ULL; // FNV-1a
			for (wchar_t c : g_installDir)
			{
				if (c >= L'A' && c <= L'Z')
					c = (wchar_t)(c + 32);
				h = (h ^ (uint32_t)c) * 1099511628211ULL;
			}

			wchar_t name[64];
			swprintf(name, 64, L"Local\\HoTStreamPatcher_%016llx", (unsigned long long)h);

			HANDLE handle = CreateMutexW(nullptr, TRUE, name);
			if (!handle)
				return true;
			if (GetLastError() == ERROR_ALREADY_EXISTS)
			{
				CloseHandle(handle);
				return false;
			}
			g_patchLock = handle;
			return true;
		}

		void Run()
		{
			g_installDir = Util::GetExeDir().wstring();
			if (!AcquirePatchLock())
			{
				StreamLog("stream: another instance is patching this install, skipping pass");
				return;
			}
			StreamLog("stream: pass start");

			std::wstring manifestUrl;
			std::string baseUrl;
			LoadLauncherUrls(g_installDir, manifestUrl, baseUrl);
			LauncherSettings settings = LoadLauncherSettings(g_installDir);

			// Bust any Cloudflare cache so we always read the newest manifest, not a stale copy.
			std::wstring manifestReq = AppendQuery(
			    manifestUrl, L"t", std::to_wstring((long long)std::time(nullptr)));

			std::string json;
			if (!HttpGetString(manifestReq, json))
			{
				StreamLog("stream: manifest fetch failed");
				return;
			}
			Manifest man;
			if (!ParseManifest(json, man))
			{
				StreamLog("stream: manifest parse failed");
				return;
			}
			if (!man.baseUrl.empty())
				baseUrl = man.baseUrl;

			{
				std::error_code ec;
				fs::create_directories(PendingFile().parent_path(), ec);
				std::ofstream(PendingFile(), std::ios::trunc | std::ios::binary);
			}
			std::atexit(&ApplyPending); // swap staged files in at clean exit, once handles are freed

			std::vector<const ManifestFile*> plan;
			long long total = 0;
			for (const ManifestFile& mf : man.files)
			{
				if (mf.hd && !settings.hdPatch)
					continue;
#ifdef AUTO_UPDATER_IGNORES_DLL
				if (IsExtensionDll(mf.path))
				{
					StreamLog("stream: AUTO_UPDATER_IGNORES_DLL set, ignoring %s", mf.path.c_str());
					continue;
				}
#endif
				std::wstring local = (fs::path(g_installDir) / fs::path(mf.path)).wstring();
				std::wstring np = StagedPath(local, IsMpq(mf.path));
				std::error_code ec;
				if (FileSize(local) == mf.size)
				{
					if (fs::exists(np, ec))
						fs::remove(np, ec);
					continue;
				}
				if (FileSize(np) == mf.size)
				{
					SwapOrStage(np, local, IsMpq(mf.path));
					continue;
				}
				plan.push_back(&mf);
				total += mf.size;
			}

			g_filesTotal = (int)plan.size();
			g_totalBytes = total;
			g_filesDone = 0;
			g_baseBytes = 0;
			g_doneBytes = 0;
			if (plan.empty())
			{
				StreamLog("stream: nothing to download");
				return;
			}

			StreamLog("stream: downloading %d file(s)", (int)plan.size());
			g_active = true;
			ActiveScope activeScope;
			for (const ManifestFile* mf : plan)
			{
				{
					std::lock_guard<std::mutex> lock(g_statusMutex);
					g_currentFile = mf->path;
				}
				PlaceFile(baseUrl, *mf, settings.maxBytesPerSecond);
				g_baseBytes = g_baseBytes.load() + mf->size;
				g_doneBytes = g_baseBytes.load();
				g_filesDone = g_filesDone.load() + 1;
				StreamLog("stream: finished %d/%d %s", g_filesDone.load(), g_filesTotal.load(), mf->path.c_str());
			}

			StreamLog("stream: background download pass complete");
		}
	}

	BackgroundDownloader& BackgroundDownloader::Instance()
	{
		static BackgroundDownloader instance;
		return instance;
	}

	void BackgroundDownloader::Start()
	{
		if (g_started.exchange(true))
			return;
		sLua.RegisterFunction("IsStreaming", &BackgroundDownloader::Lua_IsStreaming, LuaFunctionState::ALL);
		sLua.RegisterFunction("GetStreamingProgress", &BackgroundDownloader::Lua_GetProgress, LuaFunctionState::ALL);
		sLua.RegisterFunction("IsStreamingRestartPending", &BackgroundDownloader::Lua_RestartPending, LuaFunctionState::ALL);
		sLua.RegisterFunction("IsStreamingUIRefreshPending", &BackgroundDownloader::Lua_UIRefreshPending, LuaFunctionState::ALL);
		sLua.RegisterFunction("StreamingRefreshUI", &BackgroundDownloader::Lua_RefreshUI, LuaFunctionState::ALL);
		Trigger();
	}

	void BackgroundDownloader::Trigger()
	{
		bool expected = false;
		if (!g_running.compare_exchange_strong(expected, true))
			return;
		std::thread([]
		{
			Run();
			g_running = false;
		}).detach();
	}

	bool BackgroundDownloader::IsActive()
	{
		return g_active.load();
	}

	int BackgroundDownloader::Lua_IsStreaming(lua_State* L)
	{
		FrameScript::PushBoolean(L, g_active.load());
		return 1;
	}

	int BackgroundDownloader::Lua_RestartPending(lua_State* L)
	{
		FrameScript::PushBoolean(L, g_restartNeeded.load());
		return 1;
	}

	int BackgroundDownloader::Lua_UIRefreshPending(lua_State* L)
	{
		FrameScript::PushBoolean(L, g_uiRefreshNeeded.load());
		return 1;
	}

	int BackgroundDownloader::Lua_RefreshUI(lua_State* /*L*/)
	{
		g_uiRefreshNeeded = false;
		if (ClientData::ObjectManager::GetActivePlayerObject())
			*reinterpret_cast<uint8_t*>(0x00BD0791) = 1; // CGGameUI::m_reloadUIRequested
		else
			*reinterpret_cast<uint32_t*>(0x00B6AA24) = 1; // CGlueMgr::m_reload
		return 0;
	}

	int BackgroundDownloader::Lua_GetProgress(lua_State* L)
	{
		std::string current;
		{
			std::lock_guard<std::mutex> lock(g_statusMutex);
			current = g_currentFile;
		}
		FrameScript::PushNumber(L, (double)g_filesDone.load());
		FrameScript::PushNumber(L, (double)g_filesTotal.load());
		FrameScript::PushNumber(L, (double)g_doneBytes.load());
		FrameScript::PushNumber(L, (double)g_totalBytes.load());
		FrameScript::PushString(L, current.c_str());
		return 5;
	}

	struct LuaStackGuard
	{
		lua_State* L;
		int top;
		LuaStackGuard()
		    : L(FrameScript::GetContext()), top(L ? FrameScript::GetTop(L) : 0)
		{
		}
		~LuaStackGuard()
		{
			if (L)
				FrameScript::SetTop(L, top);
		}
	};

	void BackgroundDownloader::PumpMainThread()
	{
		LuaStackGuard luaGuard;

		static bool s_prevActive = false;
		bool active = g_active.load();
		if (active != s_prevActive)
		{
			s_prevActive = active;
			int id = FrameXMLExtensions::GetEventIdByName(active ? "HOT_STREAMING_STARTED" : "HOT_STREAMING_STOPPED");
			if (id >= 0)
				FrameScript::SignalEvent((uint32_t)id, "");
		}

		std::vector<std::string> batch;
		{
			std::lock_guard<std::mutex> lock(g_mountMutex);
			if (g_mountQueue.empty())
				return;
			batch.swap(g_mountQueue);
		}
		for (const std::string& path : batch)
		{
			void* hMpq = nullptr;
			bool ok = ClientData::Streaming::MountArchive(path.c_str(), kMountPriority, &hMpq);
			StreamLog("stream: mount %s -> %s", ok ? "OK" : "FAILED", path.c_str());
			if (ok && hMpq)
			{
				ClientData::Streaming::RebuildHash();
				DbcFromMpq::RefreshResult r = DbcFromMpq::RefreshFromArchive(hMpq);

				if (r.spellDataChanged && ClientData::ObjectManager::GetActivePlayerObject())
					ClientData::SpellBook::Refresh();
				if (r.interfaceFiles)
					g_uiRefreshNeeded = true;
			}
		}
	}
}
