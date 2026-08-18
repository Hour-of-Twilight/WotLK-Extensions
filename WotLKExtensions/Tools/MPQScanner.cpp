#include "MPQScanner.h"
#include "CustomLua.h"

#include "Streaming/BackgroundDownloader.h"
#include "Streaming/Sha256.h"

#include <filesystem>
#include <algorithm>
#include <cctype>
#include <windows.h>
namespace fs = std::filesystem;

int MpqScanner::GetMpqList(lua_State* L)
{
	const std::vector<MpqInfo> mpqs = sMpqScanner.GetResults();
	char buffer[512];
	for (const auto& mpq : mpqs)
	{
		SStr::Printf(buffer, sizeof(buffer), "%s %s", mpq.filename_lower.c_str(),
		    mpq.updating ? "(updating)" : mpq.digest.c_str());
		CGChat::AddChatMessage(buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}
	return 0;
}

namespace
{
	std::string ToLower(const std::string& s)
	{
		std::string out = s;
		std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c)
		{
			return std::tolower(c);
		});
		return out;
	}

	fs::path GetDataFolder()
	{
		char buffer[MAX_PATH];
		GetModuleFileNameA(nullptr, buffer, MAX_PATH);
		return fs::path(buffer).parent_path() / "Data";
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
}

std::string MpqScanner::DigestOf(const std::wstring& path)
{
	std::error_code ec;
	const long long size = (long long)fs::file_size(path, ec);
	if (ec)
		return "";
	const long long mtime = (long long)fs::last_write_time(path, ec).time_since_epoch().count();
	if (ec)
		return "";

	const std::wstring key = LowerPath(path);
	auto it = digests.find(key);
	if (it != digests.end() && it->second.size == size && it->second.mtime == mtime)
		return it->second.digest;

	std::string digest = Streaming::QuickDigestFile(path);
	if (!digest.empty())
		digests[key] = CachedDigest{ size, mtime, digest };
	return digest;
}

void MpqScanner::Start()
{
	sLua.RegisterFunction("GetMpqList", &GetMpqList, LuaFunctionState::FRAME);
}

std::vector<MpqInfo> MpqScanner::GetResults()
{
	std::lock_guard<std::mutex> lock(mutex);
	const unsigned generation = Streaming::UpdateGeneration();
	if (!done.load() || generation != scannedGeneration)
	{
		Rescan();
		scannedGeneration = generation;
		done = true;
	}
	return results;
}

void MpqScanner::Rescan()
{
	results.clear();

	fs::path dataFolder = GetDataFolder();
	if (!fs::exists(dataFolder))
		return;

	auto scanFolder = [&](const fs::path& folder)
	{
		if (!fs::exists(folder))
			return;

		for (const auto& entry : fs::directory_iterator(folder))
		{
			if (!entry.is_regular_file())
				continue;

			std::string ext = ToLower(entry.path().extension().string());
			if (ext != ".mpq")
				continue;

			// A download parked next to the real file waiting for the swap. It isn't installed
			// yet and has no row in client_mpqs, so reporting it would only draw a warning.
			if (Streaming::IsStagedName(entry.path().filename().wstring()))
				continue;

			MpqInfo info;
			info.filename_lower = ToLower(entry.path().filename().string());
			// Still reported so it counts as present, but skip the read: the copy on disk is the
			// old one and we already know it, so hashing it would only cost I/O.
			info.updating = Streaming::IsUpdatePending(entry.path().wstring());
			if (!info.updating)
				info.digest = DigestOf(entry.path().wstring());
			results.push_back(info);
		}
	};

	scanFolder(dataFolder);
	scanFolder(dataFolder / "enUS");
	scanFolder(dataFolder / "enGB");
}
