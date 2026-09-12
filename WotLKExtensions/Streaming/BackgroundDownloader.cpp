#include "BackgroundDownloader.h"

#include "LauncherManifest.h"
#include "HttpClient.h"
#include "ManifestSignature.h"
#include "Sha256.h"
#include "ArchiveManifest.h"
#include "ArchivePatcher.h"
#include "ArchiveRegistry.h"
#include "BlobContainer.h"
#include "MpqFile.h"
#include "LocalDigests.h"
#include "StreamLog.h"
#include "TextConv.h"

#include <ClientData/Streaming.h>

#include "DbcFromMpq.h"
#include <ClientData/SharedDefines.h>
#include <ClientData/Spell.h>
#include <ClientData/Achievements.h>
#include <ClientData/ObjectManager.h>
#include <ClientData/ModelCache.h>
#include <Packets/Packet.h>
#include <ClientDetours.h>
#include <ClientData/ClientFunctions.h>
#include <Config/LauncherSettings.h>
#include <Helpers/Util.h>
#include <Logger.h>
#include "Lua/CustomLua.h"
#include "Lua/XMLExtensions.h"

#include <windows.h>

#include <algorithm>
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
#include <cwctype>
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

		std::atomic<bool> g_firstPassDone{ false };
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

		std::string NarrowOem(const std::wstring& s, bool* lossy)
		{
			if (s.empty())
				return "";
			const bool utf8 = GetOEMCP() == CP_UTF8;
			const DWORD flags = utf8 ? 0 : WC_NO_BEST_FIT_CHARS;
			BOOL usedDefault = FALSE;
			BOOL* used = utf8 ? nullptr : &usedDefault;
			int n = WideCharToMultiByte(CP_OEMCP, flags, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
			std::string out(n, '\0');
			WideCharToMultiByte(CP_OEMCP, flags, s.c_str(), (int)s.size(), out.data(), n, nullptr, used);
			if (lossy && usedDefault)
				*lossy = true;
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

		// The other half of StagedPath: does this name end in the .new-<8 hex> tag it appends.
		bool HasStagedTag(const std::wstring& name)
		{
			const std::wstring marker = L".new-";
			if (name.size() < marker.size() + 8)
				return false;
			if (name.compare(name.size() - 8 - marker.size(), marker.size(), marker) != 0)
				return false;
			for (size_t i = name.size() - 8; i < name.size(); ++i)
				if (!iswxdigit(name[i]))
					return false;
			return true;
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

		fs::path PendingFile()
		{
			return fs::path(g_installDir) / "Cache" / "hotstream-pending.txt";
		}

		// What we check an already-present file against. MPQs carry a sampled digest so verifying
		// them doesn't mean reading gigabytes off disk. Everything else stays on full SHA-256.
		const std::string& ExpectedDigest(const ManifestFile& mf)
		{
			return mf.quick.empty() ? mf.sha256 : mf.quick;
		}

		void RememberPlaced(const std::wstring& path, const ManifestFile& mf, const ManifestArchive* arc)
		{
			LocalDigests::Remember(path, LocalDigests::Kind::Sha256, mf.sha256);
			LocalDigests::Remember(path, LocalDigests::Kind::Quick, mf.quick);
			if (arc)
				LocalDigests::Remember(path, LocalDigests::Kind::ContentId, arc->contentId);
		}

		bool LocalMatches(const std::wstring& path, const std::string& expected)
		{
			const LocalDigests::Kind kind = IsQuickDigest(expected) ? LocalDigests::Kind::Quick : LocalDigests::Kind::Sha256;
			std::string got;
			return LocalDigests::Get(path, kind, got) && _stricmp(got.c_str(), expected.c_str()) == 0;
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

		std::mutex g_plannedMutex;
		std::set<std::wstring> g_plannedUpdates; // lowered absolute paths this pass will replace
		std::atomic<unsigned> g_updateGeneration{ 0 };

		std::wstring LocalPath(const std::string& relPath)
		{
			return (fs::path(g_installDir) / fs::path(relPath)).wstring();
		}

		std::string g_launcherExe = "HoTLauncher.exe";

		bool IsLauncherExe(const std::string& path)
		{
			size_t slash = path.find_last_of("/\\");
			std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
			return _stricmp(name.c_str(), g_launcherExe.c_str()) == 0;
		}

		std::wstring WidenUtf8(const std::string& s)
		{
			if (s.empty())
				return L"";
			int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
			std::wstring out(n, L'\0');
			MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), n);
			return out;
		}

		// The launcher's manifest entry follows the exe the player actually runs, which they may
		// have renamed, so an in-game update lands on their copy instead of dropping a second one
		// at the manifest's name. The launcher records its path in settings.json; with no record
		// (they have never run it) the manifest path is still right.
		std::wstring TargetPath(const ManifestFile& mf)
		{
			if (!IsLauncherExe(mf.path))
				return LocalPath(mf.path);

			std::wstring recorded = WidenUtf8(sLauncherSettings.LauncherPath());
			if (recorded.empty())
				return LocalPath(mf.path);

			std::error_code ec;
			return fs::exists(recorded, ec) ? recorded : LocalPath(mf.path);
		}

		void WritePendingLocked()
		{
			std::error_code ec;
			fs::create_directories(PendingFile().parent_path(), ec);
			std::ofstream f(PendingFile(), std::ios::trunc | std::ios::binary);
			if (!f)
				return;
			bool lossy = false;
			for (const PendingMove& m : g_pendingMoves)
				f << "M|" << NarrowOem(ToBackslashes(m.src), &lossy) << "|"
				  << NarrowOem(ToBackslashes(m.dst), &lossy) << "\r\n";
			for (const std::wstring& d : g_pendingDeletes)
				f << "D|" << NarrowOem(ToBackslashes(d), &lossy) << "\r\n";
			if (lossy)
				StreamLog("stream: pending path not representable in OEM code page %u, exit swap may fail",
				    GetOEMCP());
		}

		HANDLE g_swapHold = INVALID_HANDLE_VALUE;
		constexpr const char* kSwapHoldName = "hotstream-swap.hold";

		bool WriteSwapScript(const fs::path& cmdPath, const std::string& hold,
		    const std::string& pend, bool spinFirst)
		{
			std::ofstream b(cmdPath, std::ios::trunc | std::ios::binary);
			if (!b)
				return false;

			b << "@echo off\r\n";
			b << "set tries=0\r\n";

			if (spinFirst)
			{
				b << "set spin=0\r\n";
				b << ":spin\r\n";
				b << "2>nul type nul >>\"" << hold << "\" && goto apply\r\n";
				b << "set /a spin+=1\r\n";
				b << "if %spin% lss 4000 goto spin\r\n";
			}

			b << ":wait\r\n";
			b << "2>nul type nul >>\"" << hold
			  << "\" || (ping -n 2 127.0.0.1 >nul & goto wait)\r\n";
			b << ":apply\r\n";
			b << "if not exist \"" << pend << "\" goto done\r\n";
			b << "set failed=0\r\n";
			b << "for /f \"usebackq tokens=1,2,3 delims=|\" %%a in (\"" << pend << "\") do (\r\n";
			b << "  if \"%%a\"==\"M\" if exist \"%%b\" (move /y \"%%b\" \"%%c\" >nul 2>&1 || set failed=1)\r\n";
			b << "  if \"%%a\"==\"D\" if exist \"%%b\" (del /q \"%%b\" >nul 2>&1 || set failed=1)\r\n";
			b << ")\r\n";
			b << "if \"%failed%\"==\"1\" if %tries% lss 10 "
			     "(set /a tries+=1 & ping -n 2 127.0.0.1 >nul & goto apply)\r\n";
			b << ":done\r\n";
			b << "del /q \"" << pend << "\" >nul 2>&1\r\n";
			b << "del /q \"" << hold << "\" >nul 2>&1\r\n";
			b << "del /q \"%~f0\" >nul 2>&1\r\n";
			return true;
		}

		bool SpawnSwapScript(const fs::path& cmdPath)
		{
			std::wstring cmdline = L"cmd.exe /c \"" + cmdPath.wstring() + L"\"";
			std::vector<wchar_t> cl(cmdline.begin(), cmdline.end());
			cl.push_back(0);
			STARTUPINFOW si{};
			si.cb = sizeof(si);
			si.dwFlags = STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_HIDE;
			PROCESS_INFORMATION pi{};
			if (!CreateProcessW(nullptr, cl.data(), nullptr, nullptr, FALSE,
			        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
				return false;
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			return true;
		}

		std::string HoldRef()
		{
			return std::string("%~dp0") + kSwapHoldName;
		}

		std::string PendRef()
		{
			return std::string("%~dp0") + PendingFile().filename().string();
		}

		void SpawnCloseSwapHelper()
		{
			static std::atomic<bool> spawned{ false };
			if (spawned.exchange(true))
				return;

			const fs::path cacheDir = PendingFile().parent_path();
			fs::path cmdPath = cacheDir / "hotstream-swap.cmd";
			fs::path holdPath = cacheDir / kSwapHoldName;

			g_swapHold = CreateFileW(holdPath.c_str(), GENERIC_WRITE, 0, nullptr,
			    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (g_swapHold == INVALID_HANDLE_VALUE)
			{
				StreamLog("stream: swap hold create FAILED (err %lu), relying on in-process swap at exit",
				    GetLastError());
				return;
			}

			if (!WriteSwapScript(cmdPath, HoldRef(), PendRef(), false))
				return;

			if (SpawnSwapScript(cmdPath))
				StreamLog("stream: spawned close-swap helper");
			else
				StreamLog("stream: close-swap helper spawn FAILED (err %lu)", GetLastError());
		}

		void SpawnExitSwapHelper()
		{
			static std::atomic<bool> spawned{ false };
			if (spawned.exchange(true))
				return;
			if (g_swapHold == INVALID_HANDLE_VALUE)
				return;

			fs::path cmdPath = PendingFile().parent_path() / "hotstream-swap-now.cmd";
			if (!WriteSwapScript(cmdPath, HoldRef(), PendRef(), true))
				return;

			if (SpawnSwapScript(cmdPath))
				StreamLog("stream: spawned exit swap helper");
			else
				StreamLog("stream: exit swap helper spawn FAILED (err %lu)", GetLastError());
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

		bool HasPendingMove(const std::wstring& src, const std::wstring& dst)
		{
			std::lock_guard<std::mutex> lock(g_pendingMutex);
			for (const PendingMove& m : g_pendingMoves)
				if (PathEqual(m.src, src) && PathEqual(m.dst, dst))
					return true;
			return false;
		}

		void CancelPendingMove(const std::wstring& src, const std::wstring& dst)
		{
			std::lock_guard<std::mutex> lock(g_pendingMutex);
			size_t before = g_pendingMoves.size();
			for (auto it = g_pendingMoves.begin(); it != g_pendingMoves.end();)
				it = (PathEqual(it->src, src) && PathEqual(it->dst, dst)) ? g_pendingMoves.erase(it) : it + 1;
			if (g_pendingMoves.size() != before)
				WritePendingLocked();
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

		void SwapOnClose()
		{
			{
				std::lock_guard<std::mutex> lock(g_pendingMutex);
				if (g_pendingMoves.empty() && g_pendingDeletes.empty())
					return;
			}

			ApplyPending();

			bool remaining;
			{
				std::lock_guard<std::mutex> lock(g_pendingMutex);
				remaining = !g_pendingMoves.empty() || !g_pendingDeletes.empty();
			}
			if (remaining)
				SpawnExitSwapHelper();
		}

		struct MountRequest
		{
			std::string path;
		};

		// Everything one pass produced for the main thread. Nothing in it is applied until the
		// whole pass has finished downloading, so a pass that touches several archives refreshes
		// DBCs and models once, against the newest copy of each file.
		struct PassBatch
		{
			std::vector<ArchivePatchJob> jobs;
			std::vector<MountRequest> mounts;
		};

		std::mutex g_batchMutex;
		std::deque<PassBatch> g_batchQueue;
		size_t g_frontJobsDone = 0;       // guarded by g_batchMutex
		PassBatch g_passBatch;            // background thread only, published at the end of a pass
		std::set<std::wstring> g_mounted; // path + hash, so a re-download of a path still remounts

		void PublishBatch()
		{
			if (g_passBatch.jobs.empty() && g_passBatch.mounts.empty())
				return;
			StreamLog("stream: publishing batch, %d in-place job(s), %d mount(s)",
			    (int)g_passBatch.jobs.size(), (int)g_passBatch.mounts.size());
			std::lock_guard<std::mutex> lock(g_batchMutex);
			g_batchQueue.push_back(std::move(g_passBatch));
			g_passBatch = PassBatch();
		}

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
			g_passBatch.mounts.push_back(MountRequest{ std::move(p) });
		}

		int __cdecl RefreshUnitModelCb(uint32_t guidLow, uint32_t guidHigh, void*)
		{
			uint64_t guid = ((uint64_t)guidHigh << 32) | guidLow;
			ClientData::CGObject_C* unit = ClientData::ObjectManager::GetObject(guid,
			    (ClientData::ObjectTypeMask)(ClientData::TYPEMASK_UNIT | ClientData::TYPEMASK_PLAYER));
			if (unit)
				unit->UpdateDisplayInfo(1);
			return 1;
		}

		// A mounted archive only changes what gets parsed from here on, so drop the cached copies
		// and make everything already in the world rebuild against the new files.
		void RefreshLoadedModels()
		{
			ClientData::ModelCache::Invalidate();
			ClientData::ModelCache::Purge();
			if (!ClientData::ObjectManager::GetActivePlayerObject())
				return;
			ClntObjMgr::EnumVisibleObjects(RefreshUnitModelCb, nullptr);
			StreamLog("stream: rebuilt world models after art mount");
		}

		void PlaceFile(const std::string& baseUrl, const ManifestFile& mf, const ManifestArchive* arc)
		{
			std::wstring local = TargetPath(mf);
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

			std::string got;
			DownloadOptions opt;
			opt.resumeFrom = resume;
			opt.sha256Out = &got;
			opt.bytesPerSecond = []
			{
				return sLauncherSettings.MaxDownloadBytesPerSecond();
			};
			opt.onBytes = [](long long written)
			{
				g_doneBytes = g_baseBytes.load() + written;
			};
			if (!HttpDownloadFile(Widen(url), part, opt))
			{
				StreamLog("stream: download failed %s", mf.path.c_str());
				return;
			}

			if (got.empty())
				got = Sha256File(part);
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
				// SHA-256 already matched above, so the file is the manifest's file and we can
				// record its expected digest without reading it back.
				RememberPlaced(local, mf, arc);
				if (isMpq)
					EnqueueMount(local, mf.sha256);
				return;
			}

			// Exists: swap now if unlocked, else stage a hash-named copy, mount it, swap later.
			if (MoveFileExW(part.c_str(), local.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				RememberPlaced(local, mf, arc);
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

		void SwapOrStage(const std::wstring& np, const std::wstring& local, const ManifestFile& mf,
		    const ManifestArchive* arc)
		{
			const bool isMpq = IsMpq(mf.path);
			if (MoveFileExW(np.c_str(), local.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				RememberPlaced(local, mf, arc);
				if (isMpq)
					EnqueueMount(local, mf.sha256);
				SweepStagedSiblings(local, L"");
			}
			else
			{
				if (isMpq)
					EnqueueMount(np, mf.sha256);
				RecordPending(np, local, !isMpq);
				SweepStagedSiblings(local, np);
			}
		}

		// HD off parks the file as <name>.disabled like the launcher does, so turning it back on
		// is a rename plus a hash check. False means skip the file this pass.
		bool ReconcileHdFile(const std::wstring& local, bool hdPatch, bool& restored)
		{
			const std::wstring disabled = local + L".disabled";
			std::error_code ec;

			if (!hdPatch)
			{
				if (!fs::exists(local, ec))
					return false;
				if (MoveFileExW(local.c_str(), disabled.c_str(),
				        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
					StreamLog("stream: hd off, disabled %s", NarrowAcp(local).c_str());
				else if (!HasPendingMove(local, disabled))
					RecordPending(local, disabled, true); // loaded by the client, park it on exit
				return false;
			}

			// Turned back on before the queued rename ran, so keep what is already on disk.
			CancelPendingMove(local, disabled);

			if (fs::exists(local, ec) || !fs::exists(disabled, ec))
				return true;
			if (!MoveFileExW(disabled.c_str(), local.c_str(),
			        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
				return true; // could not bring it back, fall through and re-download it
			StreamLog("stream: hd on, restored %s", NarrowAcp(local).c_str());
			restored = true;
			return true;
		}

		fs::path BlobCacheDir()
		{
			return fs::path(g_installDir) / "Cache" / "blobs";
		}

		std::wstring BlobPath(const std::string& sha)
		{
			return (BlobCacheDir() / Widen(sha)).wstring();
		}

		bool ReadBinaryFile(const std::wstring& path, std::string& out)
		{
			std::ifstream f(path, std::ios::binary | std::ios::ate);
			if (!f)
				return false;
			std::streamsize size = f.tellg();
			if (size < 0)
				return false;
			out.resize((size_t)size);
			f.seekg(0, std::ios::beg);
			return size == 0 || (bool)f.read(out.data(), size);
		}

		bool WriteBinaryFileAtomic(const std::wstring& path, const std::string& data)
		{
			std::wstring tmp = path + L".tmp";
			{
				std::ofstream f(tmp, std::ios::trunc | std::ios::binary);
				if (!f || !f.write(data.data(), (std::streamsize)data.size()))
					return false;
			}
			return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
		}

		void MarkPlaced(const std::wstring& absPath)
		{
			{
				std::lock_guard<std::mutex> lock(g_plannedMutex);
				g_plannedUpdates.erase(LowerPath(absPath));
			}
			++g_updateGeneration;
		}

		const ManifestArchive* ArchiveFor(const Manifest& man, const ManifestFile& mf)
		{
			for (const ManifestArchive& a : man.archives)
				if (_stricmp(a.path.c_str(), mf.path.c_str()) == 0 && ArchiveManifest::IsContentId(a.contentId))
					return &a;
			return nullptr;
		}

		std::string JoinUrl(const std::string& baseUrl, const std::string& rel)
		{
			std::string url = baseUrl;
			if (!url.empty() && url.back() == '/')
				url.pop_back();
			return url + "/" + rel;
		}

		std::mutex g_failureMutex;
		std::map<std::wstring, int> g_archiveFailures; // consecutive in-place failures per archive
		std::map<std::wstring, std::string> g_inPlaceApplied;
		constexpr int kInPlaceGiveUp = 3;

		int InPlaceFailures(const std::wstring& path)
		{
			std::lock_guard<std::mutex> lock(g_failureMutex);
			auto it = g_archiveFailures.find(LowerPath(path));
			return it == g_archiveFailures.end() ? 0 : it->second;
		}

		void NoteInPlaceResult(const std::wstring& path, bool ok)
		{
			std::lock_guard<std::mutex> lock(g_failureMutex);
			if (ok)
				g_archiveFailures.erase(LowerPath(path));
			else
				++g_archiveFailures[LowerPath(path)];
		}

		void ExpectInPlace(const std::wstring& path, const std::string& contentId)
		{
			std::lock_guard<std::mutex> lock(g_failureMutex);
			g_inPlaceApplied[LowerPath(path)] = contentId;
		}

		void SettleInPlace(const std::wstring& path, const std::string& localId, const std::string& relPath)
		{
			std::string expected;
			{
				std::lock_guard<std::mutex> lock(g_failureMutex);
				auto it = g_inPlaceApplied.find(LowerPath(path));
				if (it == g_inPlaceApplied.end())
					return;
				expected = std::move(it->second);
				g_inPlaceApplied.erase(it);
			}
			const bool ok = (expected == localId);
			if (!ok)
				StreamLog("stream: %s was patched in place but reads back as %.12s, not %.12s", relPath.c_str(),
				    localId.c_str(), expected.c_str());
			NoteInPlaceResult(path, ok);
		}

		void ForgetInPlace(const std::wstring& path)
		{
			std::lock_guard<std::mutex> lock(g_failureMutex);
			g_inPlaceApplied.erase(LowerPath(path));
			g_archiveFailures.erase(LowerPath(path));
		}

		enum class ArchivePlan
		{
			Current,
			InPlace,
			WholeFile,
			Skip
		};

		// Works out how to bring a manifest-carrying archive up to date. The whole-file path is
		// only taken for reasons that will not go away on their own: no local manifest to diff
		// against, files that need removing (the client cannot delete), an archive that has
		// outgrown its published size because the client only ever appends, or repeated failures
		// to write. Anything transient just skips this pass.
		constexpr long long kBloatFloor = 64LL << 20;

		ArchivePlan PlanArchive(const std::string& baseUrl, const ManifestFile& mf, const ManifestArchive& arc,
		    const std::wstring& local, ArchivePatchJob& job)
		{
			if (!mf.quick.empty() && FileSize(local) == mf.size && LocalMatches(local, mf.quick))
			{
				LocalDigests::Remember(local, LocalDigests::Kind::ContentId, arc.contentId);
				ForgetInPlace(local);
				return ArchivePlan::Current;
			}

			std::string localId;
			if (!LocalDigests::Get(local, LocalDigests::Kind::ContentId, localId))
			{
				StreamLog("stream: %s unreadable, retrying next pass", mf.path.c_str());
				return ArchivePlan::Skip;
			}
			SettleInPlace(local, localId, mf.path);
			if (localId.empty())
			{
				StreamLog("stream: %s has no (hotmanifest), whole-file update", mf.path.c_str());
				return ArchivePlan::WholeFile;
			}
			if (localId == arc.contentId)
				return ArchivePlan::Current;

			if (InPlaceFailures(local) >= kInPlaceGiveUp)
			{
				StreamLog("stream: %s failed in place %d times, whole-file update", mf.path.c_str(), kInPlaceGiveUp);
				return ArchivePlan::WholeFile;
			}

			std::string localBytes;
			std::string err;
			bool unreadable = false;
			if (!MpqFile::ReadFile(local, ArchiveManifest::kFileName, localBytes, &err, &unreadable))
			{
				if (unreadable)
				{
					StreamLog("stream: %s unreadable (%s), retrying next pass", mf.path.c_str(), err.c_str());
					return ArchivePlan::Skip;
				}
				StreamLog("stream: %s manifest unreadable (%s), whole-file update", mf.path.c_str(), err.c_str());
				return ArchivePlan::WholeFile;
			}
			if (ArchiveManifest::ContentIdOf(localBytes) == arc.contentId)
			{
				LocalDigests::Remember(local, LocalDigests::Kind::ContentId, arc.contentId);
				return ArchivePlan::Current;
			}
			std::vector<ArchiveEntry> localEntries;
			if (!ArchiveManifest::Parse(localBytes, localEntries, &err))
			{
				StreamLog("stream: %s manifest corrupt (%s), whole-file update", mf.path.c_str(), err.c_str());
				return ArchivePlan::WholeFile;
			}

			std::string targetBytes;
			std::wstring indexUrl = Widen(JoinUrl(baseUrl, BlobContainer::RelativeIndexPath(arc.contentId)));
			if (!HttpGetString(indexUrl, targetBytes))
			{
				StreamLog("stream: %s index fetch failed, retrying next pass", mf.path.c_str());
				return ArchivePlan::Skip;
			}
			if (ArchiveManifest::ContentIdOf(targetBytes) != arc.contentId)
			{
				StreamLog("stream: %s index does not hash to its contentId, retrying next pass", mf.path.c_str());
				return ArchivePlan::Skip;
			}
			std::vector<ArchiveEntry> targetEntries;
			if (!ArchiveManifest::Parse(targetBytes, targetEntries, &err))
			{
				StreamLog("stream: %s index corrupt (%s), retrying next pass", mf.path.c_str(), err.c_str());
				return ArchivePlan::Skip;
			}

			ArchiveDiff diff = ArchiveManifest::Diff(localEntries, targetEntries);
			if (!diff.removed.empty())
			{
				StreamLog("stream: %s drops %d file(s), whole-file update", mf.path.c_str(), (int)diff.removed.size());
				return ArchivePlan::WholeFile;
			}
			long long targetRaw = 0;
			for (const ArchiveEntry& e : targetEntries)
				targetRaw += e.size;
			const long long onDisk = FileSize(local);
			const long long growth = targetRaw > 0
			                             ? (long long)((double)diff.changedBytes * (double)mf.size / (double)targetRaw)
			                             : diff.changedBytes;
			const long long allowed = mf.size + std::max(kBloatFloor, mf.size / 4);
			if (onDisk + growth > allowed)
			{
				StreamLog("stream: %s would grow to %lld MB against %lld MB published, whole-file update to compact",
				    mf.path.c_str(), (onDisk + growth) >> 20, mf.size >> 20);
				return ArchivePlan::WholeFile;
			}

			job = ArchivePatchJob();
			job.archivePath = local;
			job.relPath = mf.path;
			job.targetManifest = std::move(targetBytes);
			job.contentId = arc.contentId;
			for (ArchiveEntry& e : diff.changed)
			{
				ArchiveWrite w;
				w.name = std::move(e.name);
				w.size = e.size;
				w.sha256 = e.sha256;
				w.blobPath = BlobPath(w.sha256);
				job.writes.push_back(std::move(w));
			}
			StreamLog("stream: %s -> %.12s in place, %d file(s), %lld bytes", mf.path.c_str(),
			    arc.contentId.c_str(), (int)job.writes.size(), diff.changedBytes);
			return ArchivePlan::InPlace;
		}

		// Leaves the verified raw bytes of one file at Cache/blobs/<sha>, downloading unless a
		// valid copy is already there.
		bool FetchBlob(const std::string& baseUrl, const ArchiveWrite& w)
		{
			std::string existing;
			if (FileSize(w.blobPath) == w.size && ReadBinaryFile(w.blobPath, existing) &&
			    Sha256Hex(existing.data(), existing.size()) == w.sha256)
				return true;

			std::error_code ec;
			fs::create_directories(BlobCacheDir(), ec);
			std::wstring part = w.blobPath + L".part";
			fs::remove(part, ec);

			DownloadOptions opt;
			opt.bytesPerSecond = []
			{
				return sLauncherSettings.MaxDownloadBytesPerSecond();
			};
			const long long rawSize = w.size;
			opt.onBytes = [rawSize](long long written)
			{
				g_doneBytes = g_baseBytes.load() + std::min(written, rawSize);
			};
			std::wstring url = Widen(JoinUrl(baseUrl, BlobContainer::RelativeBlobPath(w.sha256)));
			if (!HttpDownloadFile(url, part, opt))
			{
				fs::remove(part, ec);
				StreamLog("stream: blob download failed %s (%.12s)", w.name.c_str(), w.sha256.c_str());
				return false;
			}

			std::string container;
			std::string raw;
			std::string err;
			bool ok = ReadBinaryFile(part, container) && BlobContainer::Unpack(container, raw, &err);
			fs::remove(part, ec);
			if (!ok)
			{
				StreamLog("stream: blob unpack failed %s: %s", w.name.c_str(), err.c_str());
				return false;
			}
			if ((long long)raw.size() != w.size || Sha256Hex(raw.data(), raw.size()) != w.sha256)
			{
				StreamLog("stream: blob hash mismatch %s", w.name.c_str());
				return false;
			}
			if (!WriteBinaryFileAtomic(w.blobPath, raw))
			{
				StreamLog("stream: cannot write blob cache %s", w.name.c_str());
				return false;
			}
			return true;
		}

		bool FetchJobBlobs(const std::string& baseUrl, const ArchivePatchJob& job)
		{
			for (const ArchiveWrite& w : job.writes)
			{
				{
					std::lock_guard<std::mutex> lock(g_statusMutex);
					g_currentFile = job.relPath + " " + w.name;
				}
				if (!FetchBlob(baseUrl, w))
					return false;
				g_baseBytes = g_baseBytes.load() + w.size;
				g_doneBytes = g_baseBytes.load();
			}
			return true;
		}

		// Blobs left behind by a pass that never got applied. Whatever the current plan still
		// wants is kept, the rest is dead weight.
		void SweepBlobCache(const std::set<std::string>& wanted)
		{
			std::error_code ec;
			for (const fs::directory_entry& e : fs::directory_iterator(BlobCacheDir(), ec))
			{
				if (!e.is_regular_file(ec))
					continue;
				std::string name = NarrowAcp(e.path().filename().wstring());
				if (wanted.find(name) == wanted.end())
					fs::remove(e.path(), ec);
			}
		}

		struct BatchProgress
		{
			size_t nextJob = 0;
			size_t nextMount = 0;
			std::set<std::string> seen;
			std::vector<std::string> names;
			bool anyApplied = false;
		};
		BatchProgress g_batchProgress; // main thread only

		void AddRefreshName(BatchProgress& bp, const std::string& name)
		{
			if (bp.seen.insert(Text::LowerAscii(name)).second)
				bp.names.push_back(name);
		}

		// The one refresh a batch gets: every changed name is re-read through the client's normal
		// lookup, so whichever archive now wins for it is what lands in memory.
		void FinishBatch(BatchProgress& bp)
		{
			if (!bp.anyApplied)
				return;
			ClientData::Streaming::RebuildHash();
			DbcFromMpq::RefreshResult r = DbcFromMpq::RefreshNames(bp.names);
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
			if (r.artFiles)
				RefreshLoadedModels();
			StreamLog("stream: batch applied, refreshed %d name(s)", (int)bp.names.size());
		}

		// One item of the front batch per frame, then a single refresh once it is drained.
		void ProcessBatch()
		{
			PassBatch* batch = nullptr;
			{
				std::lock_guard<std::mutex> lock(g_batchMutex);
				if (g_batchQueue.empty())
					return;
				batch = &g_batchQueue.front();
			}
			BatchProgress& bp = g_batchProgress;

			if (bp.nextJob < batch->jobs.size())
			{
				const ArchivePatchJob& job = batch->jobs[bp.nextJob++];
				unsigned long long t0 = GetTickCount64();
				ArchivePatchResult r = ArchivePatcher::Apply(job);
				StreamLog("stream: in-place %s %s: %d/%d file(s) written in %llu ms%s%s",
				    job.relPath.c_str(), r.ok ? "OK" : "FAILED", (int)r.writtenNames.size(),
				    (int)job.writes.size(), GetTickCount64() - t0, r.error.empty() ? "" : ", ",
				    r.error.c_str());
				for (const std::string& n : r.writtenNames)
					AddRefreshName(bp, n);
				if (!r.writtenNames.empty())
					bp.anyApplied = true;
				if (r.ok)
					ExpectInPlace(job.archivePath, job.contentId);
				else
					NoteInPlaceResult(job.archivePath, false);
				MarkPlaced(job.archivePath);
				std::lock_guard<std::mutex> lock(g_batchMutex);
				g_frontJobsDone = bp.nextJob;
				return;
			}

			if (bp.nextMount < batch->mounts.size())
			{
				const MountRequest& req = batch->mounts[bp.nextMount++];
				int priority = g_nextMountPriority.fetch_add(1);
				void* hMpq = nullptr;
				bool ok = ClientData::Streaming::MountArchive(req.path.c_str(), priority, &hMpq);
				StreamLog("stream: mount %s (prio %d) -> %s", ok ? "OK" : "FAILED", priority, req.path.c_str());
				if (ok && hMpq)
				{
					std::vector<std::string> names;
					DbcFromMpq::CollectArchiveNames(hMpq, names);
					for (const std::string& n : names)
						AddRefreshName(bp, n);
					bp.anyApplied = true;
				}
				return;
			}

			FinishBatch(bp);
			bp = BatchProgress();
			std::lock_guard<std::mutex> lock(g_batchMutex);
			g_frontJobsDone = 0;
			g_batchQueue.pop_front();
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

		struct FirstPassScope
		{
			~FirstPassScope()
			{
				g_firstPassDone = true;
			}
		};

		void Run()
		{
			FirstPassScope firstPassScope; // login stays gated until we know what needs patching
			g_installDir = Util::GetExeDir().wstring();
			if (!AcquirePatchLock())
			{
				StreamLog("stream: another instance is patching this install, skipping pass");
				return;
			}
			StreamLog("stream: pass start");

			LauncherConfig cfg;
			LoadLauncherConfig(g_installDir, cfg);
			std::wstring manifestUrl = cfg.manifestUrl;
			std::string baseUrl = cfg.patchBaseUrl;
			g_launcherExe = cfg.launcherExe;
			const bool hdPatch = sLauncherSettings.HdPatch();

			// Bust any Cloudflare cache so we always read the newest manifest, not a stale copy.
			// The signature covers the body rather than the URL, so this doesn't affect it.
			std::wstring stamp = std::to_wstring((long long)std::time(nullptr));
			std::wstring manifestReq = AppendQuery(manifestUrl, L"t", stamp);

			std::string json;
			if (!HttpGetString(manifestReq, json))
			{
				StreamLog("stream: manifest fetch failed");
				return;
			}

			// Checked before parsing, so nothing in the manifest is acted on until the bytes are
			// known to be ours. A deploy landing between the two fetches pairs a fresh manifest
			// with a stale signature, which fails closed here and works on the next pass.
			std::wstring sigReq = AppendQuery(
			    manifestUrl + Widen(kSignatureSuffix), L"t", stamp);

			std::string signature;
			if (!HttpGetString(sigReq, signature))
			{
				StreamLog("stream: manifest signature fetch failed");
				return;
			}
			if (!VerifyManifestSignature(json, signature))
			{
				StreamLog("stream: manifest signature REJECTED, ignoring this manifest");
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
				std::atexit(&SwapOnClose); // swap staged files in at clean exit, once handles are freed
			}

			// Whatever this pass queues for the main thread goes out as one batch when the pass
			// ends, however it ends.
			struct BatchScope
			{
				~BatchScope()
				{
					PublishBatch();
				}
			} batchScope;

			struct PlannedItem
			{
				const ManifestFile* file;
				bool inPlace;
				ArchivePatchJob job;
				long long bytes;
			};
			std::vector<PlannedItem> plan;
			long long total = 0;
			std::set<std::string> wantedBlobs;
			for (const ManifestFile& mf : man.files)
			{
#ifdef AUTO_UPDATER_IGNORES_DLL
				if (IsExtensionDll(mf.path))
				{
					StreamLog("stream: AUTO_UPDATER_IGNORES_DLL set, ignoring %s", mf.path.c_str());
					continue;
				}
#endif
				std::wstring local = TargetPath(mf);

				bool restored = false;
				if (mf.hd && !ReconcileHdFile(local, hdPatch, restored))
					continue;

				std::wstring np = StagedPath(local, mf.sha256);

				// An archive that describes itself is judged by its contents, not its bytes, and
				// is brought up to date file by file where that is possible.
				const ManifestArchive* arc = IsMpq(mf.path) ? ArchiveFor(man, mf) : nullptr;
				if (arc && FileSize(local) >= 0 && FileSize(np) != mf.size)
				{
					PlannedItem item{ &mf, true, ArchivePatchJob(), 0 };
					ArchivePlan how = PlanArchive(baseUrl, mf, *arc, local, item.job);
					if (how == ArchivePlan::Current)
					{
						// Restored from .disabled, so the client never loaded it at startup.
						if (restored)
							EnqueueMount(local, arc->contentId);
						SweepStagedSiblings(local, L"");
						continue;
					}
					if (how == ArchivePlan::Skip)
						continue;
					if (how == ArchivePlan::InPlace)
					{
						for (const ArchiveWrite& w : item.job.writes)
						{
							item.bytes += w.size;
							wantedBlobs.insert(w.sha256);
						}
						total += item.bytes;
						plan.push_back(std::move(item));
						continue;
					}
				}

				if (FileSize(local) == mf.size && LocalMatches(local, ExpectedDigest(mf)))
				{
					// Restored from .disabled, so the client never loaded it at startup.
					if (restored && IsMpq(mf.path))
						EnqueueMount(local, mf.sha256);
					SweepStagedSiblings(local, L""); // nothing left to swap in
					continue;
				}
				if (FileSize(np) == mf.size)
				{
					SwapOrStage(np, local, mf, arc);
					continue;
				}
				plan.push_back(PlannedItem{ &mf, false, ArchivePatchJob(), mf.size });
				total += mf.size;
			}
			LocalDigests::Save();
			SweepBlobCache(wantedBlobs);

			// Publish what this pass is going to replace, so the sanity check can leave those files
			// alone instead of reporting a stale copy we already know about. In-place jobs still
			// waiting on the main thread from an earlier pass stay listed until they land.
			{
				std::set<std::wstring> planned;
				for (const PlannedItem& item : plan)
					planned.insert(LowerPath(TargetPath(*item.file)));
				{
					std::lock_guard<std::mutex> lock(g_batchMutex);
					for (size_t b = 0; b < g_batchQueue.size(); ++b)
					{
						const PassBatch& batch = g_batchQueue[b];
						for (size_t j = (b == 0) ? g_frontJobsDone : 0; j < batch.jobs.size(); ++j)
							planned.insert(LowerPath(batch.jobs[j].archivePath));
					}
				}

				std::lock_guard<std::mutex> lock(g_plannedMutex);
				if (planned != g_plannedUpdates)
				{
					g_plannedUpdates.swap(planned);
					++g_updateGeneration;
				}
			}

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

			StreamLog("stream: downloading %d file(s), %lld bytes", (int)plan.size(), total);
			g_active = true;
			ActiveScope activeScope;
			for (PlannedItem& item : plan)
			{
				const ManifestFile& mf = *item.file;
				{
					std::lock_guard<std::mutex> lock(g_statusMutex);
					g_currentFile = mf.path;
				}
				if (item.inPlace)
				{
					// Applied by the main thread once the whole batch is published, which is also
					// when the file stops counting as pending.
					if (FetchJobBlobs(baseUrl, item.job))
						g_passBatch.jobs.push_back(std::move(item.job));
					else
					{
						StreamLog("stream: %s in-place download incomplete, retrying next pass", mf.path.c_str());
						MarkPlaced(TargetPath(mf));
					}
				}
				else
				{
					PlaceFile(baseUrl, mf, ArchiveFor(man, mf));
					// Placed, so it is either current on disk or staged, and staged files are
					// already covered by the pending-move list.
					MarkPlaced(TargetPath(mf));
					g_baseBytes = g_baseBytes.load() + mf.size;
				}
				g_doneBytes = g_baseBytes.load();
				g_filesDone = g_filesDone.load() + 1;
				g_lastProgressMs = GetTickCount64();
				StreamLog("stream: finished %d/%d %s", g_filesDone.load(), g_filesTotal.load(), mf.path.c_str());
			}

			LocalDigests::Save();
			StreamLog("stream: background download pass complete");
		}
	}

	bool IsUpdatePending(const std::wstring& absPath)
	{
		const std::wstring key = LowerPath(absPath);
		{
			std::lock_guard<std::mutex> lock(g_plannedMutex);
			if (g_plannedUpdates.find(key) != g_plannedUpdates.end())
				return true;
		}
		std::lock_guard<std::mutex> lock(g_pendingMutex);
		for (const PendingMove& m : g_pendingMoves)
			if (LowerPath(m.dst) == key)
				return true;
		return false;
	}

	unsigned UpdateGeneration()
	{
		return g_updateGeneration.load();
	}

	bool IsStagedName(const std::wstring& fileName)
	{
		// StagedPath puts the tag before the last extension, or at the end when there isn't one.
		const std::wstring stem = fs::path(fileName).stem().wstring();
		return HasStagedTag(stem) || HasStagedTag(fileName);
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
		sLua.RegisterFunction("IsStreamingBusy", &BackgroundDownloader::Lua_IsBusy, LuaFunctionState::ALL);
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
		if (!g_started.load()) // downloader turned off, nothing to check against
			return;

		bool expected = false;
		if (!g_running.compare_exchange_strong(expected, true))
			return;
		std::thread([]
		{
			SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
			Run();
			g_running = false;
		}).detach();
	}

	bool BackgroundDownloader::IsActive()
	{
		return g_active.load();
	}

	bool BackgroundDownloader::IsBusy()
	{
		if (g_active.load())
			return true;
		return g_started.load() && !g_firstPassDone.load();
	}

	int BackgroundDownloader::Lua_IsStreaming(lua_State* L)
	{
		FrameScript::PushBoolean(L, g_active.load());
		return 1;
	}

	int BackgroundDownloader::Lua_IsBusy(lua_State* L)
	{
		FrameScript::PushBoolean(L, Instance().IsBusy());
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
			const char* eventName = active ? "HOT_STREAMING_STARTED" : "HOT_STREAMING_STOPPED";
			EnqueueEvent([eventName]
			{
				FrameXMLExtensions::SignalEvent(eventName, "");
			});
		}

		// Main-thread archive work: one batch item per frame, then a single refresh. Any Lua
		// events it produces are queued rather than fired inline.
		ProcessBatch();

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

CLIENT_DETOUR(EventPostCloseEx, 0x0047D290, __cdecl, int, (unsigned int context))
{
	::Streaming::SwapOnClose();
	return EventPostCloseEx(context);
}
