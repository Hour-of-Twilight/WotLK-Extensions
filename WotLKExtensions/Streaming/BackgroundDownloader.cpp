#include "BackgroundDownloader.h"

#include "LauncherManifest.h"
#include "HttpClient.h"
#include "Sha256.h"

#include <ClientData/Streaming.h>

#include "DbcFromMpq.h"
#include <ClientData/SharedDefines.h>
#include <ClientData/Spell.h>
#include <ClientData/Achievements.h>
#include <ClientData/ObjectManager.h>
#include <Packets/Packet.h>
#include <ClientDetours.h>
#include <ClientData/ClientFunctions.h>
#include <Config/LauncherSettings.h>
#include <Helpers/Util.h>
#include <Logger.h>
#include "Lua/CustomLua.h"
#include "Lua/XMLExtensions.h"

#include <windows.h>

#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <functional>
#include <utility>
#include <filesystem>
#include <fstream>
#include <cctype>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iterator>

namespace fs = std::filesystem;

namespace Streaming
{
	namespace
	{
		std::atomic<bool> g_started{ false };
		std::atomic<bool> g_running{ false };
		std::atomic<bool> g_pollerStarted{ false };
		std::wstring g_installDir;

		constexpr unsigned kPollIntervalMs = 5 * 60 * 1000;

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

		constexpr unsigned long long kProgressJoinMs = 30000;
		std::atomic<unsigned long long> g_lastProgressMs{ 0 };

		void ResetProgress()
		{
			g_filesDone = 0;
			g_filesTotal = 0;
			g_baseBytes = 0;
			g_doneBytes = 0;
			g_totalBytes = 0;
		}

		void StreamLog(const char* fmt, ...)
		{
			char buf[1024];
			va_list a;
			va_start(a, fmt);
			std::vsnprintf(buf, sizeof(buf), fmt, a);
			va_end(a);
			sLog.Write("DEBUG", "stream", buf);
		}

		constexpr int kMountPriorityBase = 1000000;
		std::atomic<int> g_nextMountPriority{ kMountPriorityBase };

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

		std::wstring LowerPath(std::wstring p)
		{
			for (wchar_t& c : p)
			{
				if (c == L'/')
					c = L'\\';
				else if (c >= L'A' && c <= L'Z')
					c = (wchar_t)(c + 32);
			}
			return p;
		}

		bool PathEqual(const std::wstring& a, const std::wstring& b)
		{
			return a.size() == b.size() && LowerPath(a) == LowerPath(b);
		}

		std::wstring StagedPath(const std::wstring& local, const std::string& sha)
		{
			std::wstring tag = L".new-" + Widen(sha.substr(0, 8));
			size_t dot = local.find_last_of(L'.');
			size_t slash = local.find_last_of(L"/\\");
			if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash))
				return local + tag;
			return local.substr(0, dot) + tag + local.substr(dot);
		}

		bool StartsWithNoCase(const std::wstring& s, const std::wstring& pre)
		{
			return s.size() >= pre.size() && LowerPath(s.substr(0, pre.size())) == LowerPath(pre);
		}

		bool EndsWithNoCase(const std::wstring& s, const std::wstring& suf)
		{
			return s.size() >= suf.size() &&
			    LowerPath(s.substr(s.size() - suf.size())) == LowerPath(suf);
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

		long long FileMtime(const std::wstring& p)
		{
			std::error_code ec;
			auto t = fs::last_write_time(p, ec);
			return ec ? -1 : (long long)t.time_since_epoch().count();
		}

		fs::path PendingFile()
		{
			return fs::path(g_installDir) / "Cache" / "hotstream-pending.txt";
		}

		fs::path HashCacheFile()
		{
			return fs::path(g_installDir) / "Cache" / "hotstream-local.txt";
		}

		struct HashEntry
		{
			long long size;
			long long mtime;
			std::string sha;
		};

		std::map<std::wstring, HashEntry> g_localHashes;
		bool g_localHashesDirty = false;

		void LoadLocalHashes()
		{
			g_localHashes.clear();
			g_localHashesDirty = false;
			std::ifstream in(HashCacheFile(), std::ios::binary);
			std::string line;
			while (in && std::getline(in, line))
			{
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				size_t a = line.find('|');
				size_t b = (a == std::string::npos) ? a : line.find('|', a + 1);
				size_t c = (b == std::string::npos) ? b : line.find('|', b + 1);
				if (c == std::string::npos)
					continue;
				HashEntry e;
				e.size = _atoi64(line.substr(0, a).c_str());
				e.mtime = _atoi64(line.substr(a + 1, b - a - 1).c_str());
				e.sha = line.substr(b + 1, c - b - 1);
				std::string path = line.substr(c + 1);
				if (!path.empty())
					g_localHashes[LowerPath(WidenAcp(path))] = e;
			}
		}

		void SaveLocalHashes()
		{
			if (!g_localHashesDirty)
				return;
			std::error_code ec;
			fs::create_directories(HashCacheFile().parent_path(), ec);
			std::ofstream f(HashCacheFile(), std::ios::trunc | std::ios::binary);
			if (!f)
				return;
			for (const auto& kv : g_localHashes)
				f << kv.second.size << "|" << kv.second.mtime << "|" << kv.second.sha << "|"
				  << NarrowAcp(kv.first) << "\r\n";
			g_localHashesDirty = false;
		}

		void RememberLocal(const std::wstring& path, const std::string& sha)
		{
			long long size = FileSize(path);
			long long mtime = FileMtime(path);
			if (size < 0 || mtime < 0)
				return;
			g_localHashes[LowerPath(path)] = HashEntry{ size, mtime, sha };
			g_localHashesDirty = true;
		}

		bool LocalMatches(const std::wstring& path, const std::string& sha)
		{
			long long size = FileSize(path);
			long long mtime = FileMtime(path);
			if (size < 0 || mtime < 0)
				return false;

			auto it = g_localHashes.find(LowerPath(path));
			if (it != g_localHashes.end() && it->second.size == size && it->second.mtime == mtime)
				return _stricmp(it->second.sha.c_str(), sha.c_str()) == 0;

			std::string got = Sha256File(path);
			if (got.empty())
				return false;
			g_localHashes[LowerPath(path)] = HashEntry{ size, mtime, got };
			g_localHashesDirty = true;
			return _stricmp(got.c_str(), sha.c_str()) == 0;
		}

		std::wstring ToBackslashes(std::wstring p)
		{
			for (wchar_t& c : p)
				if (c == L'/')
					c = L'\\';
			return p;
		}

		struct PendingMove
		{
			std::wstring src;
			std::wstring dst;
		};

		std::mutex g_pendingMutex;
		std::vector<PendingMove> g_pendingMoves;
		std::vector<std::wstring> g_pendingDeletes;

		void WritePendingLocked()
		{
			std::error_code ec;
			fs::create_directories(PendingFile().parent_path(), ec);
			std::ofstream f(PendingFile(), std::ios::trunc | std::ios::binary);
			if (!f)
				return;
			for (const PendingMove& m : g_pendingMoves)
				f << "M|" << NarrowAcp(ToBackslashes(m.src)) << "|" << NarrowAcp(ToBackslashes(m.dst))
				  << "\r\n";
			for (const std::wstring& d : g_pendingDeletes)
				f << "D|" << NarrowAcp(ToBackslashes(d)) << "\r\n";
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
				b << "for /f \"usebackq tokens=1,2,3 delims=|\" %%a in (\"" << pend << "\") do (\r\n";
				b << "  if \"%%a\"==\"M\" move /y \"%%b\" \"%%c\" >nul 2>&1\r\n";
				b << "  if \"%%a\"==\"D\" del /q \"%%b\" >nul 2>&1\r\n";
				b << ")\r\n";
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

		void RecordPending(const std::wstring& newPath, const std::wstring& target, bool needsRestart)
		{
			{
				std::lock_guard<std::mutex> lock(g_pendingMutex);
				for (auto it = g_pendingMoves.begin(); it != g_pendingMoves.end();)
				{
					if (!PathEqual(it->dst, target))
					{
						++it;
						continue;
					}
					if (!PathEqual(it->src, newPath))
					{
						StreamLog("stream: superseding staged %s", NarrowAcp(it->src).c_str());
						g_pendingDeletes.push_back(it->src);
					}
					it = g_pendingMoves.erase(it);
				}
				g_pendingMoves.push_back(PendingMove{ newPath, target });
				WritePendingLocked();
			}
			if (needsRestart)
				g_restartNeeded = true;
			SpawnCloseSwapHelper();
		}

		void DropStaged(const std::wstring& path)
		{
			std::error_code ec;
			if (!fs::exists(path, ec))
				return;

			bool queued = false;
			{
				std::lock_guard<std::mutex> lock(g_pendingMutex);
				for (auto it = g_pendingMoves.begin(); it != g_pendingMoves.end();)
					it = PathEqual(it->src, path) ? g_pendingMoves.erase(it) : it + 1;

				if (fs::remove(path, ec))
				{
					StreamLog("stream: removed stale staged %s", NarrowAcp(path).c_str());
				}
				else
				{
					bool known = false;
					for (const std::wstring& d : g_pendingDeletes)
						known = known || PathEqual(d, path);
					if (!known)
						g_pendingDeletes.push_back(path);
					queued = true;
				}
				WritePendingLocked();
			}
			if (queued)
			{
				StreamLog("stream: stale staged %s in use, deleting on exit", NarrowAcp(path).c_str());
				SpawnCloseSwapHelper();
			}
		}

		void SweepStagedSiblings(const std::wstring& local, const std::wstring& keep)
		{
			fs::path lp(local);
			std::wstring prefix = lp.stem().wstring() + L".new-";
			std::wstring ext = lp.extension().wstring();

			std::error_code ec;
			std::vector<std::wstring> stale;
			for (const fs::directory_entry& e : fs::directory_iterator(lp.parent_path(), ec))
			{
				if (!e.is_regular_file(ec))
					continue;
				std::wstring name = e.path().filename().wstring();
				if (name.size() <= prefix.size() + ext.size())
					continue;
				if (!StartsWithNoCase(name, prefix) || !EndsWithNoCase(name, ext))
					continue;
				if (!PathEqual(e.path().wstring(), keep))
					stale.push_back(e.path().wstring());
			}
			for (const std::wstring& s : stale)
				DropStaged(s);
		}

		void ApplyPending()
		{
			std::lock_guard<std::mutex> lock(g_pendingMutex);
			for (auto it = g_pendingMoves.begin(); it != g_pendingMoves.end();)
			{
				std::error_code ec;
				if (!fs::exists(it->src, ec) ||
				    MoveFileExW(it->src.c_str(), it->dst.c_str(),
				        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
					it = g_pendingMoves.erase(it);
				else
					++it;
			}
			for (auto it = g_pendingDeletes.begin(); it != g_pendingDeletes.end();)
			{
				std::error_code ec;
				if (!fs::exists(*it, ec) || fs::remove(*it, ec))
					it = g_pendingDeletes.erase(it);
				else
					++it;
			}
			WritePendingLocked();
		}

		std::mutex g_mountMutex;
		std::vector<std::string> g_mountQueue;
		std::set<std::wstring> g_mounted; // path + hash, so a re-download of a path still remounts

		std::mutex g_eventMutex;
		std::deque<std::function<void()>> g_eventQueue;

		void EnqueueEvent(std::function<void()> fn)
		{
			std::lock_guard<std::mutex> lock(g_eventMutex);
			g_eventQueue.push_back(std::move(fn));
		}

		void EnqueueMount(const std::wstring& path, const std::string& sha)
		{
			std::string p = NarrowAcp(path);
			if (!g_mounted.insert(LowerPath(path) + L"|" + Widen(sha)).second)
			{
				StreamLog("stream: already mounted %s", p.c_str());
				return;
			}
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
				RememberLocal(local, mf.sha256);
				if (isMpq)
					EnqueueMount(local, mf.sha256);
				return;
			}

			// Exists: swap now if unlocked, else stage a hash-named copy, mount it, swap later.
			if (MoveFileExW(part.c_str(), local.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				RememberLocal(local, mf.sha256);
				if (isMpq)
					EnqueueMount(local, mf.sha256);
				SweepStagedSiblings(local, L"");
			}
			else
			{
				std::wstring np = StagedPath(local, mf.sha256);
				if (!MoveFileExW(part.c_str(), np.c_str(), MOVEFILE_REPLACE_EXISTING))
				{
					fs::remove(part, ec);
					StreamLog("stream: could not stage %s", mf.path.c_str());
					return;
				}
				if (isMpq)
					EnqueueMount(np, mf.sha256);
				RecordPending(np, local, !isMpq);
				SweepStagedSiblings(local, np);
				StreamLog("stream: staged %s (in use), will swap on next launch", mf.path.c_str());
			}
		}

		void SwapOrStage(const std::wstring& np, const std::wstring& local, bool isMpq,
		    const std::string& sha)
		{
			if (MoveFileExW(np.c_str(), local.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				RememberLocal(local, sha);
				if (isMpq)
					EnqueueMount(local, sha);
				SweepStagedSiblings(local, L"");
			}
			else
			{
				if (isMpq)
					EnqueueMount(np, sha);
				RecordPending(np, local, !isMpq);
				SweepStagedSiblings(local, np);
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
			if (g_patchLock)
				return true;

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
			const bool hdPatch = sLauncherSettings.HdPatch();
			const long long maxBytesPerSecond = sLauncherSettings.MaxDownloadBytesPerSecond();

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

			static std::atomic<bool> s_pendingReset{ false };
			if (!s_pendingReset.exchange(true))
			{
				std::error_code ec;
				fs::create_directories(PendingFile().parent_path(), ec);
				std::ofstream(PendingFile(), std::ios::trunc | std::ios::binary);
				std::atexit(&ApplyPending); // swap staged files in at clean exit, once handles are freed
			}

			LoadLocalHashes();

			std::vector<const ManifestFile*> plan;
			long long total = 0;
			for (const ManifestFile& mf : man.files)
			{
				if (mf.hd && !hdPatch)
					continue;
#ifdef AUTO_UPDATER_IGNORES_DLL
				if (IsExtensionDll(mf.path))
				{
					StreamLog("stream: AUTO_UPDATER_IGNORES_DLL set, ignoring %s", mf.path.c_str());
					continue;
				}
#endif
				std::wstring local = (fs::path(g_installDir) / fs::path(mf.path)).wstring();
				std::wstring np = StagedPath(local, mf.sha256);

				if (FileSize(local) == mf.size && LocalMatches(local, mf.sha256))
				{
					SweepStagedSiblings(local, L""); // nothing left to swap in
					continue;
				}
				if (FileSize(np) == mf.size)
				{
					SwapOrStage(np, local, IsMpq(mf.path), mf.sha256);
					continue;
				}
				plan.push_back(&mf);
				total += mf.size;
			}
			SaveLocalHashes();

			if (plan.empty())
			{
				ResetProgress();
				StreamLog("stream: nothing to download");
				return;
			}

			unsigned long long nowMs = GetTickCount64();
			unsigned long long lastMs = g_lastProgressMs.load();
			if (lastMs == 0 || nowMs - lastMs > kProgressJoinMs)
				ResetProgress();

			g_filesTotal = g_filesTotal.load() + (int)plan.size();
			g_totalBytes = g_totalBytes.load() + total;
			g_baseBytes = g_doneBytes.load();

			StreamLog("stream: downloading %d file(s)", (int)plan.size());
			g_active = true;
			ActiveScope activeScope;
			for (const ManifestFile* mf : plan)
			{
				{
					std::lock_guard<std::mutex> lock(g_statusMutex);
					g_currentFile = mf->path;
				}
				PlaceFile(baseUrl, *mf, maxBytesPerSecond);
				g_baseBytes = g_baseBytes.load() + mf->size;
				g_doneBytes = g_baseBytes.load();
				g_filesDone = g_filesDone.load() + 1;
				g_lastProgressMs = GetTickCount64();
				StreamLog("stream: finished %d/%d %s", g_filesDone.load(), g_filesTotal.load(), mf->path.c_str());
			}

			SaveLocalHashes();
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
		StartPolling();
	}

	void BackgroundDownloader::StartPolling()
	{
		if (g_pollerStarted.exchange(true))
			return;
		std::thread([]
		{
			for (;;)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs));
				StreamLog("stream: periodic update check");
				Instance().Trigger();
			}
		}).detach();
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

	void BackgroundDownloader::PumpMainThread()
	{
		// Queue the active-state change instead of signalling it inline.
		static bool s_prevActive = false;
		bool active = g_active.load();
		if (active != s_prevActive)
		{
			s_prevActive = active;
			int eventId = FrameXMLExtensions::GetEventIdByName(
			    active ? "HOT_STREAMING_STARTED" : "HOT_STREAMING_STOPPED");
			EnqueueEvent([eventId]
			{
				if (eventId >= 0)
					FrameScript::SignalEvent((uint32_t)eventId, "");
			});
		}

		// Do the main-thread mount work, queuing any resulting Lua events rather than firing them.
		std::vector<std::string> batch;
		{
			std::lock_guard<std::mutex> lock(g_mountMutex);
			batch.swap(g_mountQueue);
		}
		for (const std::string& path : batch)
		{
			int priority = g_nextMountPriority.fetch_add(1);
			void* hMpq = nullptr;
			bool ok = ClientData::Streaming::MountArchive(path.c_str(), priority, &hMpq);
			StreamLog("stream: mount %s (prio %d) -> %s", ok ? "OK" : "FAILED", priority, path.c_str());
			if (ok && hMpq)
			{
				ClientData::Streaming::RebuildHash();
				DbcFromMpq::RefreshResult r = DbcFromMpq::RefreshFromArchive(hMpq);

				if (r.spellDataChanged && ClientData::ObjectManager::GetActivePlayerObject())
					EnqueueEvent([]
					{
						ClientData::SpellBook::Refresh();
					});
				// Streamed achievement rows land in the DBCs but not in CGAchievementInfo's index.
				if (r.achievementDataChanged)
					ClientData::Achievements::RequestRebuild();
				if (r.interfaceFiles)
					g_uiRefreshNeeded = true;
			}
		}

		if (ClientData::Achievements::ConsumeRebuildRequest() &&
		    ClientData::ObjectManager::GetActivePlayerObject())
		{
			ClientData::Achievements::RebuildIndex();
			Packet(CMSG_ACHIEVEMENT_DATA_REQUEST).Send();
			StreamLog("stream: rebuilt achievement index, requested achievement data");
		}

		// Fire exactly one queued event per update.
		std::function<void()> event;
		{
			std::lock_guard<std::mutex> lock(g_eventMutex);
			if (g_eventQueue.empty())
				return;
			event = std::move(g_eventQueue.front());
			g_eventQueue.pop_front();
		}
		event();
	}
}
